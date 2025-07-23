
"use client";

import type { FileContent } from "@/lib/types";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Skeleton } from "@/components/ui/skeleton";
import { FileContentViewer } from "./file-content-viewer";
import { Badge } from "./ui/badge";
import { ServerCrash, FileSearch, Loader } from "lucide-react";

interface FileDisplayProps {
  fileContent: FileContent | null;
  isLoading: boolean;
  isDataLoading: boolean;
  error: string | null;
}

export function FileDisplay({ fileContent, isLoading, isDataLoading, error }: FileDisplayProps) {
  if (isLoading) {
    return (
      <div className="flex flex-col items-center justify-center h-full text-muted-foreground p-4 text-center">
        <Loader className="w-12 h-12 mb-4 animate-spin" />
        <h3 className="font-semibold">Loading File...</h3>
        <p className="text-sm">Please wait while the file content is being loaded.</p>
      </div>
    );
  }

  if (error) {
     return (
      <div className="flex flex-col items-center justify-center h-full p-4 text-center">
        <ServerCrash className="w-16 h-16 text-destructive mb-4" />
        <h2 className="text-2xl font-headline font-semibold mb-2">Error Loading File</h2>
        <p className="text-muted-foreground">{error}</p>
      </div>
    );
  }

  if (!fileContent) {
    return (
      <div className="flex flex-col items-center justify-center h-full p-4 text-center">
        <FileSearch className="w-16 h-16 text-muted-foreground mb-4" />
        <h2 className="text-2xl font-headline font-semibold mb-2">Select a file</h2>
        <p className="text-muted-foreground">Choose a file from the list to view its contents.</p>
      </div>
    );
  }

  return (
    <Card className="h-full flex flex-col">
      <CardHeader>
        <CardTitle className="font-headline flex items-center justify-between">
          <span className="truncate">{fileContent.name}</span>
          <Badge variant="outline">{fileContent.extension}</Badge>
        </CardTitle>
      </CardHeader>
      <CardContent className="flex-grow h-0">
          <FileContentViewer fileContent={fileContent} isDataLoading={isDataLoading} />
      </CardContent>
    </Card>
  );
}
