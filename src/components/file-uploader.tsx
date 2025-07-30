
"use client";

import { useState, useRef, useCallback } from "react";
import { Button } from "./ui/button";
import { Progress } from "./ui/progress";
import { UploadCloud, X } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { formatBytes } from "@/lib/utils";

const CHUNK_SIZE = 1024 * 512; // 512KB

interface FileUploaderProps {
    onUploadComplete: () => void;
}

export function FileUploader({ onUploadComplete }: FileUploaderProps) {
  const [file, setFile] = useState<File | null>(null);
  const [uploadProgress, setUploadProgress] = useState(0);
  const [isUploading, setIsUploading] = useState(false);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const { toast } = useToast();

  const resetState = useCallback(() => {
    setFile(null);
    setUploadProgress(0);
    setIsUploading(false);
    if (fileInputRef.current) {
        fileInputRef.current.value = "";
    }
  }, []);
  
  const handleFileChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    const files = event.target.files;
    if (files && files.length > 0) {
      setFile(files[0]);
    }
  };

  const uploadFileInChunks = useCallback(async () => {
    if (!file) return;

    setIsUploading(true);
    setUploadProgress(0);

    const totalChunks = Math.ceil(file.size / CHUNK_SIZE);
    // Create a unique but simple identifier for the file.
    const fileIdentifier = `${file.name.replace(/[^a-zA-Z0-9-._]/g, '_')}-${file.size}-${file.lastModified}`;
    
    try {
        for (let chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
            const start = chunkIndex * CHUNK_SIZE;
            const end = Math.min(start + CHUNK_SIZE, file.size);
            const chunk = file.slice(start, end);
            
            const response = await fetch('/api/upload', {
                method: "POST",
                headers: {
                    // This is the same custom header format the SIM7600 uses.
                    "x-userdata": `X-File-ID: ${fileIdentifier}; X-Chunk-Index: ${chunkIndex}; X-Total-Chunks: ${totalChunks}; X-Original-Filename: ${file.name}`,
                    "Content-Type": "application/octet-stream",
                },
                body: chunk,
            });

            if (!response.ok) {
                const errorData = await response.json();
                throw new Error(errorData.error || "Chunk upload failed");
            }

            setUploadProgress(((chunkIndex + 1) / totalChunks) * 100);
        }
        
        setTimeout(() => {
            toast({
                title: "Upload Successful",
                description: "Your file has been processed and is now available.",
            });
            onUploadComplete();
            resetState();
        }, 1000);

    } catch (error) {
        console.error("Upload failed:", error);
        toast({
            variant: "destructive",
            title: "Upload Failed",
            description: error instanceof Error ? error.message : "An unknown error occurred.",
        });
        resetState();
    }
  }, [file, onUploadComplete, resetState, toast]);

  const handleButtonClick = () => {
    if (fileInputRef.current) {
      fileInputRef.current.click();
    }
  };

  return (
    <div className="p-2 space-y-2">
      <input
        type="file"
        ref={fileInputRef}
        onChange={handleFileChange}
        className="hidden"
        disabled={isUploading}
      />
      
      {!file && (
        <Button
            variant="outline"
            className="w-full"
            onClick={handleButtonClick}
            disabled={isUploading}
        >
            <UploadCloud className="mr-2" />
            Upload a File
        </Button>
      )}

      {file && !isUploading && (
        <div className="space-y-2">
            <div className="flex items-center justify-between text-sm p-2 border rounded-md">
                <span className="truncate pr-2">{file.name} ({formatBytes(file.size)})</span>
                <Button variant="ghost" size="icon" className="h-6 w-6" onClick={() => resetState()}>
                    <X className="h-4 w-4" />
                </Button>
            </div>
            <Button className="w-full" onClick={uploadFileInChunks}>
                Start Upload
            </Button>
        </div>
      )}

      {isUploading && (
        <div className="space-y-2 text-center">
            <p className="text-sm text-muted-foreground">Uploading {file?.name}...</p>
            <Progress value={uploadProgress} />
            <p className="text-xs text-muted-foreground">{Math.round(uploadProgress)}%</p>
        </div>
      )}
    </div>
  );
}
