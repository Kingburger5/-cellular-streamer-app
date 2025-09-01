"use client";

import { useState, useTransition, useCallback, useEffect } from "react";
import type { UploadedFile, FileContent } from "@/lib/types";
import { deleteFileAction, processFileAction, getDownloadUrlAction } from "@/app/actions";
import { getClientFiles, clientStorage } from "@/lib/firebase-client";
import { ref, getBlob } from "firebase/storage";
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

// Helper to convert ArrayBuffer to Base64
function arrayBufferToBase64(buffer: ArrayBuffer) {
  let binary = '';
  const bytes = new Uint8Array(buffer);
  const len = bytes.byteLength;
  for (let i = 0; i < len; i++) {
    binary += String.fromCharCode(bytes[i]);
  }
  return window.btoa(binary);
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

  const refreshFileList = useCallback(async (showToast = false) => {
    startFileListTransition(async () => {
      setFileListError(null);
      try {
        const updatedFiles = await getClientFiles();
        setFiles(updatedFiles);

        if (showToast) {
            toast({ title: "File list refreshed", description: `Found ${updatedFiles.length} files.` });
        }

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
  }, [selectedFileName, toast]);

  useEffect(() => {
    refreshFileList();
    // We only want to run this once on mount.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Listen for new uploads from Firebase
  useEffect(() => {
    const channel = new BroadcastChannel('new-upload');
    const onNewUpload = () => {
        console.log("New upload detected, refreshing file list...");
        refreshFileList(true);
    };
    channel.addEventListener('message', onNewUpload);
    return () => {
        channel.removeEventListener('message', onNewUpload);
        channel.close();
    }
  }, [refreshFileList]);


  useEffect(() => {
    if (selectedFileName) {
      startTransition(async () => {
        const selectedFile = files.find(f => f.name === selectedFileName);
        if (!selectedFile) {
            setError(`Could not find file details for ${selectedFileName}`);
            return;
        }

        let fileChunkBase64: string | null = null;
        try {
            const extension = selectedFile.name.split('.').pop()?.toLowerCase() || '';
            const isBinary = ['wav', 'mp3', 'ogg', 'zip', 'gz', 'bin'].includes(extension);

            if (isBinary && selectedFile.size > 0) {
                const CHUNK_SIZE = 1024 * 1024; // 1MB
                const start = Math.max(0, selectedFile.size - CHUNK_SIZE);
                
                console.log(`[CLIENT] Downloading range starting at ${start} for ${selectedFile.name}`);
                const fileRef = ref(clientStorage, `uploads/${selectedFile.name}`);
                const blob = await getBlob(fileRef, start); // Download from start to end
                const arrayBuffer = await blob.arrayBuffer();
                fileChunkBase64 = arrayBufferToBase64(arrayBuffer);
                console.log("[CLIENT] Successfully downloaded chunk and encoded to Base64.");
            } else if (selectedFile.size > 0) {
                // For non-binary (text) files, download the whole thing
                const fileRef = ref(clientStorage, `uploads/${selectedFile.name}`);
                const blob = await getBlob(fileRef);
                const arrayBuffer = await blob.arrayBuffer();
                fileChunkBase64 = arrayBufferToBase64(arrayBuffer);
            }
        } catch (e: any) {
             console.error("[CLIENT] Error downloading file chunk:", e);
             setError(`Failed to download file from storage. Server logs may have more details. Ensure Firebase credentials are set correctly. Error code: ${e.code}`);
             setActiveFileContent(null);
             return;
        }

        const result = await processFileAction(selectedFileName, fileChunkBase64);
        
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
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [selectedFileName]);

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

  const handleDownloadFile = useCallback(async (name: string) => {
    toast({ title: `Preparing '${name}' for download...` });
    const result = await getDownloadUrlAction(name);
    if ('url' in result) {
      // Create a temporary link to trigger the download
      const link = document.createElement('a');
      link.href = result.url;
      link.target = "_blank"; // Open in new tab to avoid navigation issues
      link.download = name; // This attribute is not always respected but good to have
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
      toast({ title: "Download started" });
    } else {
      toast({ title: "Download Failed", description: result.error, variant: "destructive" });
    }
  }, [toast]);

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
                    Please ensure your Storage Security Rules and CORS configuration in the Firebase Console are correct.
                </AlertDescription>
             </Alert>
           </div>
        ) : (
          <FileList
            files={files}
            selectedFile={selectedFileName}
            onSelectFile={handleSelectFile}
            onDeleteFile={handleDeleteFile}
            onDownloadFile={handleDownloadFile}
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