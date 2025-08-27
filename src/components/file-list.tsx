
"use client";

import type { UploadedFile } from "@/lib/types";
import { formatBytes } from "@/lib/utils";
import {
  SidebarHeader,
  SidebarMenu,
  SidebarMenuItem,
  SidebarMenuButton,
  SidebarMenuAction,
  SidebarTrigger,
  SidebarContent,
  SidebarFooter
} from "@/components/ui/sidebar";
import { Button } from "./ui/button";
import { Antenna, Trash2 } from "lucide-react";
import { FileIcon } from "./file-icon";
import { FileUploader } from "./file-uploader";
import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
  AlertDialogTrigger,
} from "@/components/ui/alert-dialog"


interface FileListProps {
  files: UploadedFile[];
  selectedFile: string | null;
  onSelectFile: (name: string) => void;
  onUploadStart: () => void;
  onUploadComplete: (file: any) => void; // A bit generic, but needed for the processed file
  onDeleteFile: (name: string) => void;
}

export function FileList({ files, selectedFile, onSelectFile, onUploadStart, onUploadComplete, onDeleteFile }: FileListProps) {

  const handleDelete = (e: React.MouseEvent, filename: string) => {
    e.stopPropagation();
    onDeleteFile(filename);
  }

  return (
    <>
      <SidebarHeader>
        <div className="flex items-center justify-between">
            <div className="flex items-center gap-2">
                <Antenna className="w-8 h-8 text-primary" />
                <h1 className="text-2xl font-headline font-bold">Cellular Data</h1>
            </div>
            <SidebarTrigger />
        </div>
      </SidebarHeader>
      <SidebarContent>
        <SidebarMenu className="flex-grow">
          {files.map((file) => (
            <SidebarMenuItem key={file.name}>
              <SidebarMenuButton
                onClick={() => onSelectFile(file.name)}
                isActive={selectedFile === file.name}
                className="h-auto py-2 px-2 flex flex-col items-start"
              >
                <div className="flex items-center gap-2 w-full">
                  <FileIcon filename={file.name} className="w-4 h-4 shrink-0" />
                  <span className="truncate flex-1 text-left">{file.name}</span>
                </div>
                <div className="text-xs text-muted-foreground w-full mt-1 flex justify-between">
                    <span>{file.uploadDate.toLocaleDateString()}</span>
                    <span>{formatBytes(file.size)}</span>
                </div>
              </SidebarMenuButton>

              <AlertDialog>
                <AlertDialogTrigger asChild>
                   <SidebarMenuAction showOnHover>
                      <Trash2 />
                   </SidebarMenuAction>
                </AlertDialogTrigger>
                <AlertDialogContent onClick={(e) => e.stopPropagation()}>
                    <AlertDialogHeader>
                        <AlertDialogTitle>Are you sure?</AlertDialogTitle>
                        <AlertDialogDescription>
                            This action cannot be undone. This will permanently delete the file
                            <strong className="mx-1">{file.name}</strong>
                             and remove its data from our servers.
                        </AlertDialogDescription>
                    </AlertDialogHeader>
                    <AlertDialogFooter>
                        <AlertDialogCancel>Cancel</AlertDialogCancel>
                        <AlertDialogAction onClick={(e) => handleDelete(e, file.name)} className="bg-destructive hover:bg-destructive/90">
                            Delete
                        </AlertDialogAction>
                    </AlertDialogFooter>
                </AlertDialogContent>
              </AlertDialog>

            </SidebarMenuItem>
          ))}
          {files.length === 0 && (
            <div className="text-center text-muted-foreground p-4">
                No files uploaded yet. Upload a file to begin.
            </div>
          )}
        </SidebarMenu>
      </SidebarContent>
      <SidebarFooter>
        <FileUploader onUploadStart={onUploadStart} onUploadComplete={onUploadComplete} />
      </SidebarFooter>
    </>
  );
}
