
"use client";

import { useState, useRef, useCallback } from "react";
import { Button } from "./ui/button";
import { Progress } from "./ui/progress";
import { UploadCloud, X } from "lucide-react";
import { useToast } from "@/hooks/use-toast";
import { formatBytes } from "@/lib/utils";
import { processUploadedFileAction } from "@/app/actions";

interface FileUploaderProps {
    onUploadStart: () => void;
    onUploadComplete: (processedFile: any) => void;
}

export function FileUploader({ onUploadStart, onUploadComplete }: FileUploaderProps) {
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
    setUploadProgress(0); // For visual feedback, though it will be quick
    onUploadStart();

    try {
        const fileBuffer = await file.arrayBuffer();
        setUploadProgress(50); // Mark progress

        const processedFile = await processUploadedFileAction(fileBuffer, file.name);

        setUploadProgress(100);

        if (processedFile) {
            toast({
                title: "Processing Complete",
                description: "Your file has been analyzed and the data is now visible.",
            });
            onUploadComplete(processedFile);
        } else {
            throw new Error("The server could not process the file.");
        }

    } catch (error) {
        console.error("Upload & processing failed:", error);
        toast({
            variant: "destructive",
            title: "Processing Failed",
            description: error instanceof Error ? error.message : "An unknown error occurred.",
        });
    } finally {
       resetState();
    }
  }, [file, onUploadStart, onUploadComplete, resetState, toast]);

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
            Upload & Process File
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
                Start Processing
            </Button>
        </div>
      )}

      {isUploading && (
        <div className="space-y-2 text-center">
            <p className="text-sm text-muted-foreground">Processing {file?.name}...</p>
            <Progress value={uploadProgress} />
            <p className="text-xs text-muted-foreground">{Math.round(uploadProgress)}%</p>
        </div>
      )}
    </div>
  );
}
