
"use client";

import { useState, useRef, useCallback } from "react";
import { Button } from "./ui/button";
import { Progress } from "./ui/progress";
import { UploadCloud, X } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { formatBytes } from "@/lib/utils";

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

  const handleUpload = useCallback(async () => {
    if (!file) return;

    setIsUploading(true);
    setUploadProgress(0);

    const xhr = new XMLHttpRequest();
    xhr.open("POST", "/api/upload", true);

    xhr.upload.onprogress = (event) => {
        if (event.lengthComputable) {
            const percentComplete = (event.loaded / event.total) * 100;
            setUploadProgress(percentComplete);
        }
    };
    
    xhr.onload = () => {
        setIsUploading(false);
        if (xhr.status === 200) {
            onUploadComplete();
        } else {
             try {
                const response = JSON.parse(xhr.responseText);
                toast({
                    title: `Upload Failed (Status: ${xhr.status})`,
                    description: response.details || response.error || "An unknown server error occurred.",
                    variant: "destructive"
                });
             } catch (e) {
                 toast({
                    title: `Upload Failed (Status: ${xhr.status})`,
                    description: "An unexpected, non-JSON response was received from the server.",
                    variant: "destructive"
                });
             }
        }
        resetState();
    };

    xhr.onerror = () => {
        setIsUploading(false);
        toast({
            title: "Upload Failed",
            description: "A network error occurred. Please check your connection.",
            variant: "destructive",
        });
        resetState();
    };

    xhr.setRequestHeader('X-Original-Filename', file.name);
    xhr.setRequestHeader('Content-Type', file.type || 'application/octet-stream');
    xhr.send(file);
    
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
            Upload File
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
            <Button className="w-full" onClick={handleUpload}>
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
