
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
  SidebarFooter,
  SidebarMenuSkeleton
} from "@/components/ui/sidebar";
import { Antenna, Trash2, Download } from "lucide-react";
import { FileIcon } from "./file-icon";
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
  onDeleteFile: (name: string) => void;
  onDownloadFile: (name: string) => void;
  isLoading: boolean;
}

export function FileList({ files, selectedFile, onSelectFile, onDeleteFile, onDownloadFile, isLoading }: FileListProps) {

  const handleDelete = (e: React.MouseEvent, filename: string) => {
    e.stopPropagation();
    onDeleteFile(filename);
  }

  const handleDownload = (e: React.MouseEvent, filename: string) => {
    e.stopPropagation();
    onDownloadFile(filename);
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
          {isLoading && (
            <>
                <SidebarMenuSkeleton showIcon={true}/>
                <SidebarMenuSkeleton showIcon={true}/>
                <SidebarMenuSkeleton showIcon={true}/>
            </>
          )}
          {!isLoading && files.map((file) => (
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

              <div className="flex items-center gap-1.5">
                 <SidebarMenuAction showOnHover onClick={(e) => handleDownload(e, file.name)}>
                    <Download />
                 </SidebarMenuAction>
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
                                 from Firebase Storage.
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
              </div>

            </SidebarMenuItem>
          ))}
          {!isLoading && files.length === 0 && (
            <div className="text-center text-muted-foreground p-4">
                No files in storage. Awaiting new data from cellular module.
            </div>
          )}
        </SidebarMenu>
      </SidebarContent>
      <SidebarFooter>
        <div className="p-4 text-xs text-center text-muted-foreground">
            Uploads are handled exclusively by the cellular transmission module.
        </div>
      </SidebarFooter>
    </>
  );
}
