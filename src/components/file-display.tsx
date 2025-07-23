"use client";

import type { FileContent } from "@/lib/types";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Skeleton } from "@/components/ui/skeleton";
import { FileContentViewer } from "./file-content-viewer";
import { Badge } from "./ui/badge";
import { ServerCrash, FileSearch } from "lucide-react";

interface FileDisplayProps {
  fileContent: FileContent | null;
  isLoading: boolean;
  error: string | null;
}

export function FileDisplay({ fileContent, isLoading, error }: FileDisplayProps) {
  if (isLoading) {
    return (
      <Card className="h-full flex flex-col">
        <CardHeader>
          <Skeleton className="h-8 w-3/4" />
          <Skeleton className="h-4 w-1/4" />
        </CardHeader>
        <CardContent className="flex-grow">
          <Skeleton className="h-full w-full" />
        </CardContent>
      </Card>
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
          <FileContentViewer fileContent={fileContent} />
      </CardContent>
    </Card>
  );
}
