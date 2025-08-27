
"use client";

import type { FileContent } from "@/lib/types";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { WaveFileViewer } from "./wave-file-viewer";
import { Badge } from "./ui/badge";
import { ServerCrash, FileSearch, Loader } from "lucide-react";

interface FileDisplayProps {
  fileContent: FileContent | null;
  isLoading: boolean;
  error: string | null;
}

export function FileDisplay({ fileContent, isLoading, error }: FileDisplayProps) {
  if (isLoading) {
    return (
      <Card className="h-full flex flex-col items-center justify-center text-muted-foreground p-4 text-center">
        <Loader className="w-12 h-12 mb-4 animate-spin" />
        <h3 className="font-semibold">Processing File...</h3>
        <p className="text-sm">Please wait while the file is being uploaded and analyzed.</p>
      </Card>
    );
  }

  if (error) {
     return (
      <Card className="h-full flex flex-col items-center justify-center p-4 text-center">
        <ServerCrash className="w-16 h-16 text-destructive mb-4" />
        <h2 className="text-2xl font-headline font-semibold mb-2">Error Processing File</h2>
        <p className="text-muted-foreground">{error}</p>
      </Card>
    );
  }

  if (!fileContent) {
    return (
      <Card className="h-full flex flex-col items-center justify-center p-4 text-center">
        <FileSearch className="w-16 h-16 text-muted-foreground mb-4" />
        <h2 className="text-2xl font-headline font-semibold mb-2">No file selected</h2>
        <p className="text-muted-foreground">Upload a file to view its contents and analysis.</p>
      </Card>
    );
  }

  // The WaveFileViewer now contains the Tabs and handles all different views.
  // This simplifies the FileDisplay component.
  return (
    <Card className="h-full flex flex-col">
      <CardHeader>
        <CardTitle className="font-headline flex items-center justify-between">
          <span className="truncate">{fileContent.name}</span>
          <Badge variant="outline">{fileContent.extension}</Badge>
        </CardTitle>
      </CardHeader>
      <CardContent className="flex-grow h-0">
          <WaveFileViewer fileContent={fileContent} />
      </CardContent>
    </Card>
  );
}
