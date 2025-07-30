
"use client";

import { useState, useTransition, useCallback, useEffect } from "react";
import type { UploadedFile, FileContent, DataPoint } from "@/lib/types";
import { getFilesAction, getFileContentAction, deleteFileAction, getExtractedDataAction } from "@/app/actions";
import { SidebarProvider, Sidebar, SidebarInset } from "@/components/ui/sidebar";
import { FileList } from "./file-list";
import { FileDisplay } from "./file-display";
import { useToast } from "@/hooks/use-toast";

interface MainViewProps {
  initialFiles: UploadedFile[];
}

export function MainView({ initialFiles }: MainViewProps) {
  const [files, setFiles] = useState<UploadedFile[]>(initialFiles);
  const [selectedFileName, setSelectedFileName] = useState<string | null>(null);
  const [fileContent, setFileContent] = useState<FileContent | null>(null);
  const [extractedData, setExtractedData] = useState<DataPoint[] | null>(null);
  const [error, setError] = useState<string | null>(null);

  const [isLoadingContent, startContentTransition] = useTransition();
  const [isLoadingData, startDataTransition] = useTransition();
  const [isRefreshing, startRefreshTransition] = useTransition();

  const { toast } = useToast();

  const handleRefresh = useCallback(async (isAutoRefresh = false) => {
      const refreshedFiles = await getFilesAction();
      setFiles(refreshedFiles);
      if (!isAutoRefresh) {
        toast({
          title: "File list updated",
          description: `Found ${refreshedFiles.length} files.`,
        });
      }
      return refreshedFiles;
  }, [toast]);

  const handleSelectFile = useCallback((name: string) => {
    if (name === selectedFileName) return;

    setSelectedFileName(name);
    setFileContent(null);
    setExtractedData(null);
    setError(null);

    startContentTransition(async () => {
      const contentResult = await getFileContentAction(name);
      if (!contentResult) {
        setError(`Failed to load content for ${name}.`);
        toast({
          variant: "destructive",
          title: "Error",
          description: `Could not load content for ${name}.`,
        });
        return;
      }
      setFileContent(contentResult);
      
      startDataTransition(async () => {
        const data = await getExtractedDataAction(contentResult.rawMetadata);
        setExtractedData(data);
      });
    });
  }, [toast, selectedFileName]);

  const handleUploadComplete = useCallback(async () => {
    startRefreshTransition(async () => {
        const refreshedFiles = await handleRefresh(true);
        if (refreshedFiles.length > 0 && refreshedFiles.length > files.length) {
            const newFile = refreshedFiles[0]; // Just select the newest file
            if (newFile) {
                handleSelectFile(newFile.name);
            }
        }
    });
  }, [handleRefresh, handleSelectFile, files.length]);

  useEffect(() => {
    if (fileContent && extractedData) {
        setFileContent(current => current ? {...current, extractedData: extractedData} : null);
    }
  }, [extractedData, fileContent]);

  const handleDeleteFile = useCallback((name: string) => {
     startRefreshTransition(async () => {
        const result = await deleteFileAction(name);
        if (result.success) {
            toast({
                title: "File Deleted",
                description: `Successfully deleted ${name.substring(name.indexOf('-') + 1)}.`,
            });
            if (selectedFileName === name) {
                setSelectedFileName(null);
                setFileContent(null);
                setError(null);
                setExtractedData(null);
            }
            await handleRefresh(true);
        } else {
            toast({
                variant: "destructive",
                title: "Delete Failed",
                description: result.error,
            });
        }
     });
  }, [toast, handleRefresh, selectedFileName]);

  const onRefresh = useCallback(() => {
    startRefreshTransition(async () => {
        await handleRefresh(false);
    });
  }, [handleRefresh]);

  return (
    <SidebarProvider>
      <Sidebar className="flex flex-col">
        <FileList
          files={files}
          selectedFile={selectedFileName}
          onSelectFile={handleSelectFile}
          onRefresh={onRefresh}
          isRefreshing={isRefreshing}
          onUploadComplete={handleUploadComplete}
          onDeleteFile={handleDeleteFile}
        />
      </Sidebar>
      <SidebarInset className="p-0 h-screen overflow-hidden">
        <div className="h-full w-full p-4">
            <FileDisplay
                fileContent={fileContent}
                isLoading={isLoadingContent}
                isDataLoading={isLoadingData}
                error={error}
            />
        </div>
      </SidebarInset>
    </SidebarProvider>
  );
}
