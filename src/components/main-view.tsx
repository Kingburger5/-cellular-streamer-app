
"use client";

import { useState, useTransition, useCallback } from "react";
import type { UploadedFile, FileContent } from "@/lib/types";
import { deleteFileAction } from "@/app/actions";
import { SidebarProvider, Sidebar, SidebarInset } from "@/components/ui/sidebar";
import { FileList } from "./file-list";
import { FileDisplay } from "./file-display";
import { useToast } from "@/hooks/use-toast";

interface MainViewProps {
  initialFiles: UploadedFile[]; // This will be empty now but kept for structure
}

export function MainView({ initialFiles }: MainViewProps) {
  // Instead of a list of filenames, we now store the full FileContent object of processed files.
  const [processedFiles, setProcessedFiles] = useState<FileContent[]>([]);
  const [selectedFile, setSelectedFile] = useState<FileContent | null>(null);

  const [isProcessing, startTransition] = useTransition();

  const { toast } = useToast();

  const handleSelectFile = useCallback((name: string) => {
    const file = processedFiles.find(f => f.name === name);
    if (file) {
      setSelectedFile(file);
    }
  }, [processedFiles]);


  const handleUploadStart = () => {
    startTransition(() => {
        // Clear the current view to show a loading state
        setSelectedFile(null);
    });
  }

  const handleUploadComplete = useCallback((newFile: FileContent) => {
    startTransition(() => {
        // Add new file to the beginning of the list and select it
        setProcessedFiles(currentFiles => [newFile, ...currentFiles]);
        setSelectedFile(newFile);
    });
  }, []);


  const handleDeleteFile = useCallback((name: string) => {
     startTransition(async () => {
        setProcessedFiles(currentFiles => currentFiles.filter(f => f.name !== name));
        if (selectedFile?.name === name) {
            const newSelection = processedFiles.length > 1 ? processedFiles[1] : null;
            setSelectedFile(newSelection);
        }
        // We call the action on the backend even though it does nothing,
        // in case we want to add server-side cleanup later (e.g. from a database).
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
      size: f.content.length, 
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
        <FileDisplay
            fileContent={selectedFile}
            isLoading={isProcessing}
            error={null} // Error handling is now part of the upload process
        />
      </SidebarInset>
    </SidebarProvider>
  );
}
