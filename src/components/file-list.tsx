"use client";

import type { UploadedFile } from "@/lib/types";
import { formatBytes } from "@/lib/utils";
import {
  SidebarHeader,
  SidebarMenu,
  SidebarMenuItem,
  SidebarMenuButton,
  SidebarTrigger,
  SidebarContent,
} from "@/components/ui/sidebar";
import { Button } from "./ui/button";
import { RefreshCw, Antenna } from "lucide-react";
import { FileIcon } from "./file-icon";

interface FileListProps {
  files: UploadedFile[];
  selectedFile: string | null;
  onSelectFile: (name: string) => void;
  onRefresh: () => void;
  isRefreshing: boolean;
}

export function FileList({ files, selectedFile, onSelectFile, onRefresh, isRefreshing }: FileListProps) {
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
        <div className="p-2">
            <Button variant="outline" size="sm" className="w-full" onClick={onRefresh} disabled={isRefreshing}>
                <RefreshCw className={`w-4 h-4 mr-2 ${isRefreshing ? 'animate-spin' : ''}`} />
                Refresh Files
            </Button>
        </div>
        <SidebarMenu>
          {files.map((file) => (
            <SidebarMenuItem key={file.name}>
              <SidebarMenuButton
                onClick={() => onSelectFile(file.name)}
                isActive={selectedFile === file.name}
                className="h-auto py-2 px-2 flex flex-col items-start"
              >
                <div className="flex items-center gap-2 w-full">
                  <FileIcon filename={file.name} className="w-4 h-4 shrink-0" />
                  <span className="truncate flex-1 text-left">{file.name.substring(file.name.indexOf('-') + 1)}</span>
                </div>
                <div className="text-xs text-muted-foreground w-full mt-1 flex justify-between">
                    <span>{new Date(file.uploadDate).toLocaleString()}</span>
                    <span>{formatBytes(file.size)}</span>
                </div>
              </SidebarMenuButton>
            </SidebarMenuItem>
          ))}
          {files.length === 0 && (
            <div className="text-center text-muted-foreground p-4">
                No files uploaded yet.
            </div>
          )}
        </SidebarMenu>
      </SidebarContent>
    </>
  );
}
