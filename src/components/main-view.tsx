
"use client";

import { useState, useTransition, useCallback } from "react";
import type { UploadedFile, FileContent } from "@/lib/types";
import { deleteFileAction } from "@/app/actions";
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
  initialFiles: UploadedFile[]; // This will be empty now but kept for structure
}

export function MainView({ initialFiles }: MainViewProps) {
  // Instead of a list of filenames, we now store the full FileContent object of processed files.
  const [processedFiles, setProcessedFiles] = useState<FileContent[]>([]);
  const [selectedFile, setSelectedFile] = useState<FileContent | null>(null);

  const [isProcessing, startTransition] = useTransition();
  const [error, setError] = useState<string | null>(null);

  const { toast } = useToast();

  const handleSelectFile = useCallback((name: string) => {
    const file = processedFiles.find(f => f.name === name);
    if (file) {
      setSelectedFile(file);
    }
  }, [processedFiles]);


  const handleUploadStart = () => {
    startTransition(() => {
        setError(null);
        setSelectedFile(null);
    });
  }

  const handleUploadComplete = useCallback((newFile: FileContent | null, error?: string) => {
    if (error) {
        startTransition(() => {
            setError(error);
            setProcessedFiles([]); // Clear any previous files on error
            setSelectedFile(null);
        });
        return;
    }

    if (newFile) {
        startTransition(() => {
            setProcessedFiles(currentFiles => [newFile, ...currentFiles]);
            setSelectedFile(newFile); // This ensures the new file is immediately displayed
            setError(null);
        });
    }
  }, []);


  const handleDeleteFile = useCallback((name: string) => {
     startTransition(async () => {
        const remainingFiles = processedFiles.filter(f => f.name !== name);
        setProcessedFiles(remainingFiles);

        if (selectedFile?.name === name) {
            // Select the next file in the list, or null if it's empty
            const newSelection = remainingFiles.length > 0 ? remainingFiles[0] : null;
            setSelectedFile(newSelection);
        }

        await deleteFileAction(name);
        toast({
            title: "File Removed",
            description: `Removed ${name} from the list.`,
        });
     });
  }, [toast, selectedFile, processedFiles]);
  
  // Map FileContent to the UploadedFile format expected by FileList
  const fileListItems: UploadedFile[] = processedFiles.map(f => ({
      name: f.name,
      // Approximate size, as we don't have the raw file buffer here anymore
      size: f.isBinary ? (f.content.length * 0.75) : f.content.length, // Base64 approx
      uploadDate: new Date(), // Use current date as we don't store this persistently
  }));

  return (
    <SidebarProvider>
      <Sidebar className="flex flex-col">
        <FileList
          files={fileListItems}
          selectedFile={selectedFile?.name || null}
          onSelectFile={handleSelectFile}
          onUploadStart={handleUploadStart}
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
                    fileContent={selectedFile}
                    isLoading={isProcessing}
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
