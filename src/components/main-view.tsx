
"use client";

import { useState, useTransition, useCallback, useEffect } from "react";
import type { UploadedFile, FileContent } from "@/lib/types";
import { deleteFileAction, processFileAction } from "@/app/actions";
import { getClientFiles } from "@/lib/firebase-client";
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
import { Alert, AlertTitle, AlertDescription } from "./ui/alert";
import { Terminal } from "lucide-react";


interface MainViewProps {
  initialFiles: UploadedFile[];
}

export function MainView({ initialFiles }: MainViewProps) {
  const [files, setFiles] = useState<UploadedFile[]>(initialFiles);
  const [selectedFileName, setSelectedFileName] = useState<string | null>(null);
  const [activeFileContent, setActiveFileContent] = useState<FileContent | null>(null);
  const [isLoading, startTransition] = useTransition();
  const [isFileListLoading, startFileListTransition] = useTransition();
  const [error, setError] = useState<string | null>(null);
  const [fileListError, setFileListError] = useState<string | null>(null);

  const { toast } = useToast();

  const handleSelectFile = useCallback((name: string) => {
    setSelectedFileName(name);
    setError(null);
  }, []);

  const refreshFileList = useCallback(async () => {
    startFileListTransition(async () => {
      setFileListError(null);
      try {
        const updatedFiles = await getClientFiles();
        setFiles(updatedFiles);
        if (selectedFileName && !updatedFiles.some(f => f.name === selectedFileName)) {
            setSelectedFileName(null);
        } else if (!selectedFileName && updatedFiles.length > 0) {
            setSelectedFileName(updatedFiles[0].name);
        }
      } catch (err: any) {
        setFileListError(err.message || "An unknown error occurred while fetching files.");
      }
    });
  }, [selectedFileName]);

  useEffect(() => {
    refreshFileList();
    // We only want to run this once on mount.
    // eslint-disable-next-line react-hooks/exhaustive-deps
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
        {fileListError ? (
           <div className="p-4">
             <Alert variant="destructive">
                <Terminal className="h-4 w-4" />
                <AlertTitle>Could not load files!</AlertTitle>
                <AlertDescription>
                    {fileListError}
                    <br/><br/>
                    Please ensure you have set your Storage Security Rules in the Firebase Console.
                </AlertDescription>
             </Alert>
           </div>
        ) : (
          <FileList
            files={files}
            selectedFile={selectedFileName}
            onSelectFile={handleSelectFile}
            onUploadComplete={handleUploadComplete}
            onDeleteFile={handleDeleteFile}
            isLoading={isFileListLoading}
          />
        )}
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
