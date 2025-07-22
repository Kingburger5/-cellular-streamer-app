"use client";

import type { FileContent } from "@/lib/types";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Skeleton } from "@/components/ui/skeleton";
import { FileContentViewer } from "./file-content-viewer";
import { Badge } from "./ui/badge";
import { Sparkles, ServerCrash, FileSearch } from "lucide-react";
import { ScrollArea } from "./ui/scroll-area";

interface FileDisplayProps {
  fileContent: FileContent | null;
  summary: string | null;
  isLoading: boolean;
  error: string | null;
}

export function FileDisplay({ fileContent, summary, isLoading, error }: FileDisplayProps) {
  if (isLoading) {
    return (
      <div className="grid md:grid-cols-2 gap-4 h-full p-4">
        <Card className="h-full flex flex-col">
          <CardHeader>
            <Skeleton className="h-8 w-3/4" />
            <Skeleton className="h-4 w-1/4" />
          </CardHeader>
          <CardContent className="flex-grow">
            <Skeleton className="h-full w-full" />
          </CardContent>
        </Card>
        <Card className="h-full flex flex-col">
          <CardHeader>
            <Skeleton className="h-8 w-1/2" />
          </CardHeader>
          <CardContent className="flex-grow">
             <div className="space-y-4">
              <Skeleton className="h-4 w-full" />
              <Skeleton className="h-4 w-full" />
              <Skeleton className="h-4 w-5/6" />
            </div>
          </CardContent>
        </Card>
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
        <p className="text-muted-foreground">Choose a file from the list to view its contents and summary.</p>
      </div>
    );
  }

  return (
    <div className="grid lg:grid-cols-2 gap-4 h-full">
      <Card className="h-full flex flex-col">
        <CardHeader>
          <CardTitle className="font-headline flex items-center justify-between">
            <span>{fileContent.name}</span>
            <Badge variant="outline">{fileContent.extension}</Badge>
          </CardTitle>
        </CardHeader>
        <CardContent className="flex-grow h-0">
          <ScrollArea className="h-full">
            <FileContentViewer fileContent={fileContent} />
          </ScrollArea>
        </CardContent>
      </Card>

      <Card className="h-full flex flex-col">
        <CardHeader>
          <CardTitle className="font-headline flex items-center gap-2">
            <Sparkles className="text-accent" />
            <span>AI Summary</span>
          </CardTitle>
          <CardDescription>An AI-generated summary of the file content.</CardDescription>
        </CardHeader>
        <CardContent className="flex-grow">
          <ScrollArea className="h-full">
            {summary ? <p className="text-sm whitespace-pre-wrap">{summary}</p> : <p className="text-sm text-muted-foreground">No summary available or an error occurred.</p>}
          </ScrollArea>
        </CardContent>
      </Card>
    </div>
  );
}
