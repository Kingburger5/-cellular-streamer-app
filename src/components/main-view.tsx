"use client";

import { useState, useTransition, useCallback } from "react";
import type { UploadedFile, FileContent } from "@/lib/types";
import { getFilesAction, getFileContentAction, deleteFileAction } from "@/app/actions";
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
  const [error, setError] = useState<string | null>(null);

  const [isLoading, startTransition] = useTransition();
  const [isRefreshing, startRefreshTransition] = useTransition();

  const { toast } = useToast();

  const handleRefresh = useCallback(async (selectFile?: string) => {
      const refreshedFiles = await getFilesAction();
      setFiles(refreshedFiles);
      if (!selectFile) {
        toast({
          title: "File list updated",
          description: `Found ${refreshedFiles.length} files.`,
        });
      }
      return refreshedFiles;
  }, [toast]);

  const handleUploadComplete = useCallback(async () => {
    startRefreshTransition(async () => {
        toast({
            title: "Upload Successful",
            description: "Your file has been uploaded and processed.",
        });
        const refreshedFiles = await handleRefresh();
        if (refreshedFiles.length > 0) {
            // Find the newest file to select it
            const newFile = refreshedFiles.reduce((latest, file) => new Date(file.uploadDate) > new Date(latest.uploadDate) ? file : latest);
            handleSelectFile(newFile.name);
        }
    });
  }, [toast, handleRefresh]);


  const handleSelectFile = useCallback((name: string) => {
    setSelectedFileName(name);
    setFileContent(null);
    setError(null);

    startTransition(async () => {
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
    });
  }, [toast]);

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
            }
            await handleRefresh(name);
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
        await handleRefresh();
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
                isLoading={isLoading}
                error={error}
            />
        </div>
      </SidebarInset>
    </SidebarProvider>
  );
}
