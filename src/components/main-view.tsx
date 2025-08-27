
"use client";

import { useState, useTransition, useCallback, useEffect } from "react";
import type { UploadedFile, FileContent } from "@/lib/types";
import { deleteFileAction, processFileAction, getFilesAction } from "@/app/actions";
import { SidebarProvider, Sidebar, SidebarInset } from "@/components/ui/sidebar";
import { FileList } from "./file-list";
import { FileDisplay } from "./file-display";
import { useToast } from "@/hooks/use-toast";
import {
  Tabs,
  TabsContent,
  TabsList,
  TabsTrigger,
} from "@/components/ui/tabs"
import { DebugLogViewer } from "./debug-log-viewer";


interface MainViewProps {
  initialFiles: UploadedFile[];
}

export function MainView({ initialFiles }: MainViewProps) {
  const [files, setFiles] = useState<UploadedFile[]>(initialFiles);
  const [selectedFileName, setSelectedFileName] = useState<string | null>(initialFiles[0]?.name || null);
  const [activeFileContent, setActiveFileContent] = useState<FileContent | null>(null);

  const [isLoading, startTransition] = useTransition();
  const [error, setError] = useState<string | null>(null);

  const { toast } = useToast();

  const handleSelectFile = useCallback((name: string) => {
    setSelectedFileName(name);
    setError(null);
  }, []);

  useEffect(() => {
    if (selectedFileName) {
      startTransition(async () => {
        setActiveFileContent(null); // Clear previous content
        const content = await processFileAction(selectedFileName);
        if (content) {
          setActiveFileContent(content);
        } else {
          setError(`Failed to process file: ${selectedFileName}`);
        }
      });
    } else {
        setActiveFileContent(null);
    }
  }, [selectedFileName]);

  const refreshFileList = useCallback(async () => {
      const updatedFiles = await getFilesAction();
      setFiles(updatedFiles);
  }, []);

  const handleUploadComplete = useCallback(() => {
    toast({ title: "Upload Successful", description: "File sent to server. Refreshing list..." });
    refreshFileList();
  }, [refreshFileList, toast]);

  const handleDeleteFile = useCallback(async (name: string) => {
     startTransition(async () => {
        const result = await deleteFileAction(name);
        if (result.success) {
            toast({
                title: "File Deleted",
                description: `${name} has been deleted from storage.`,
            });
            await refreshFileList();
            if (selectedFileName === name) {
                setSelectedFileName(null);
            }
        } else {
             toast({
                title: "Deletion Failed",
                description: result.error,
                variant: "destructive"
            });
        }
     });
  }, [toast, refreshFileList, selectedFileName]);

  return (
    <SidebarProvider>
      <Sidebar className="flex flex-col">
        <FileList
          files={files}
          selectedFile={selectedFileName}
          onSelectFile={handleSelectFile}
          onUploadComplete={handleUploadComplete}
          onDeleteFile={handleDeleteFile}
        />
      </Sidebar>
      <SidebarInset className="p-4 h-screen overflow-hidden">
          <Tabs defaultValue="main" className="h-full w-full flex flex-col">
            <div className="flex justify-end">
                <TabsList>
                    <TabsTrigger value="main">File Viewer</TabsTrigger>
                    <TabsTrigger value="logs">Connection Log</TabsTrigger>
                </TabsList>
            </div>
            <TabsContent value="main" className="flex-grow h-0 mt-4">
                 <FileDisplay
                    fileContent={activeFileContent}
                    isLoading={isLoading}
                    error={error}
                />
            </TabsContent>
            <TabsContent value="logs" className="flex-grow h-0 mt-4">
                <DebugLogViewer />
            </TabsContent>
        </Tabs>
      </SidebarInset>
    </SidebarProvider>
  );
}
