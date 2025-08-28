
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
    if (name === selectedFileName) return; // Don't re-process if the same file is clicked
    setSelectedFileName(name);
    setError(null);
    setActiveFileContent(null); // Clear previous content immediately
  }, [selectedFileName]);

  const refreshFileList = useCallback(async () => {
    startFileListTransition(async () => {
      setFileListError(null);
      try {
        const updatedFiles = await getClientFiles();
        setFiles(updatedFiles);
        // If no file is selected, or the selected file was deleted, select the first one.
        if ((!selectedFileName && updatedFiles.length > 0) || (selectedFileName && !updatedFiles.some(f => f.name === selectedFileName))) {
          const newFileToSelect = updatedFiles[0]?.name || null;
          setSelectedFileName(newFileToSelect);
          if (!newFileToSelect) {
            // If there are no files left, clear the display
            setActiveFileContent(null);
            setError(null);
          }
        } else if (updatedFiles.length === 0) {
            // All files were deleted
            setSelectedFileName(null);
            setActiveFileContent(null);
            setError(null);
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
        const result = await processFileAction(selectedFileName);
        if (result && 'error' in result) {
            setError(result.error);
            setActiveFileContent(null);
        } else if (result) {
            setActiveFileContent(result);
            setError(null);
        } else {
            setError(`Failed to process file: ${selectedFileName}. Result was null.`);
            setActiveFileContent(null);
        }
      });
    } else {
        // If no file is selected, clear the content.
        setActiveFileContent(null);
        setError(null);
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
        } else {
             toast({
                title: "Deletion Failed",
                description: result.error,
                variant: "destructive"
            });
        }
     });
  }, [toast, refreshFileList]);

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
                    Please ensure your Storage Security Rules in the Firebase Console are correct.
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
      <SidebarInset className="p-4 h-screen overflow-y-auto">
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
                    isLoading={isLoading && !activeFileContent} // Show loading only when there's no stale content
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
