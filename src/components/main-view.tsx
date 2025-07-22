"use client";

import { useState, useTransition, useCallback } from "react";
import type { UploadedFile, FileContent } from "@/lib/types";
import { getFilesAction, getFileContentAction, generateSummaryAction } from "@/app/actions";
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
  const [summary, setSummary] = useState<string | null>(null);
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
    setSummary(null);
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

      const summaryResult = await generateSummaryAction({
        filename: contentResult.name,
        fileContent: contentResult.content,
      });

      if ("error" in summaryResult) {
        setSummary(summaryResult.error);
        toast({
          variant: "destructive",
          title: "AI Summary Error",
          description: summaryResult.error,
        });
      } else {
        setSummary(summaryResult.summary);
      }
    });
  }, [toast]);

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
        />
      </Sidebar>
      <SidebarInset className="p-0 h-screen overflow-hidden">
        <div className="h-full w-full">
            <FileDisplay
                fileContent={fileContent}
                summary={summary}
                isLoading={isLoading}
                error={error}
            />
        </div>
      </SidebarInset>
    </SidebarProvider>
  );
}
