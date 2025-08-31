
"use client";

import type { FileContent } from "@/lib/types";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Badge } from "./ui/badge";
import { ServerCrash, FileSearch, Loader, Download } from "lucide-react";
import { Button } from "./ui/button";
import { getDownloadUrlAction } from "@/app/actions";
import { useToast } from "@/hooks/use-toast";
import { DataVisualizer } from "./data-visualizer";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { SummaryViewer } from "./summary-viewer";

interface FileDisplayProps {
  fileContent: FileContent | null;
  isLoading: boolean;
  error: string | null;
}

export function FileDisplay({ fileContent, isLoading, error }: FileDisplayProps) {
  const { toast } = useToast();

  const handleDownload = async () => {
    if (!fileContent) return;

    toast({ title: "Preparing download..." });
    const result = await getDownloadUrlAction(fileContent.name);
    if ('url' in result) {
      // Create a temporary link to trigger the download
      const link = document.createElement('a');
      link.href = result.url;
      link.target = "_blank"; // Open in new tab to avoid navigation issues
      link.download = fileContent.name; // This attribute is not always respected but good to have
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
      toast({ title: "Download started" });
    } else {
      toast({ title: "Download Failed", description: result.error, variant: "destructive" });
    }
  };

  if (isLoading) {
    return (
      <Card className="h-full flex flex-col items-center justify-center text-muted-foreground p-4 text-center">
        <Loader className="w-12 h-12 mb-4 animate-spin" />
        <h3 className="font-semibold">Loading & Processing File...</h3>
        <p className="text-sm">Please wait while the file is retrieved and analyzed by the AI.</p>
      </Card>
    );
  }

  if (error) {
     return (
      <Card className="h-full flex flex-col items-center justify-center p-4 text-center">
        <ServerCrash className="w-16 h-16 text-destructive mb-4" />
        <h2 className="text-2xl font-headline font-semibold mb-2">Error Processing File</h2>
        <p className="text-muted-foreground mb-4">{error}</p>
        <p className="text-sm text-muted-foreground">Please check the server logs for more details.</p>
      </Card>
    );
  }

  if (!fileContent) {
    return (
      <Card className="h-full flex flex-col items-center justify-center p-4 text-center">
        <FileSearch className="w-16 h-16 text-muted-foreground mb-4" />
        <h2 className="text-2xl font-headline font-semibold mb-2">No file selected</h2>
        <p className="text-muted-foreground">Select a file from the list to view its contents and analysis.</p>
      </Card>
    );
  }
  
  const audioSrc = fileContent.isBinary ? fileContent.content : undefined;
  const hasRawMetadata = !!fileContent.rawMetadata;
  const hasData = !!fileContent.extractedData && fileContent.extractedData.length > 0;
  
  // The data is loading if the AI analysis hasn't populated the extractedData field yet.
  const isDataLoading = fileContent.rawMetadata ? !fileContent.extractedData : false;

  // Determine the best default tab to show.
  let defaultTab = "summary";
  if (hasData) {
      defaultTab = "visualization";
  } else if (!hasRawMetadata && audioSrc) {
      defaultTab = "audio";
  } else if (!hasData && hasRawMetadata) {
      defaultTab = "metadata";
  }


  return (
    <Card className="h-full flex flex-col">
      <CardHeader>
        <div className="flex justify-between items-start">
            <div className="flex-1">
                <CardTitle className="font-headline flex items-center gap-2">
                  <span className="truncate">{fileContent.name}</span>
                  <Badge variant="outline">{fileContent.extension}</Badge>
                </CardTitle>
                <CardDescription>File from persistent storage. Select a tab to view details.</CardDescription>
            </div>
            <Button variant="outline" size="sm" onClick={handleDownload}>
                <Download className="mr-2" />
                Download
            </Button>
        </div>
      </CardHeader>
      <CardContent className="flex-grow h-0">
          <Tabs defaultValue={defaultTab} className="h-full flex flex-col">
              <TabsList className="shrink-0">
                  <TabsTrigger value="summary" disabled={!hasRawMetadata}>AI Summary</TabsTrigger>
                  <TabsTrigger value="visualization" disabled={!hasRawadata}>Data Visualization</TabsTrigger>
                  <TabsTrigger value="audio" disabled={!audioSrc}>Audio Playback</TabsTrigger>
                  <TabsTrigger value="metadata" disabled={!hasRawMetadata}>Raw Metadata</TabsTrigger>
              </TabsList>
              <TabsContent value="summary" className="flex-grow h-0 mt-4">
                  <SummaryViewer fileContent={fileContent} />
              </TabsContent>
              <TabsContent value="visualization" className="flex-grow h-0 mt-4">
                  <DataVisualizer data={fileContent.extractedData} fileName={fileContent.name} isLoading={isDataLoading}/>
              </TabsContent>
              <TabsContent value="audio" className="flex-grow h-0 mt-4">
                  <Card className="h-full">
                      <CardHeader>
                          <CardTitle>Audio Playback</CardTitle>
                      </CardHeader>
                      <CardContent>
                          {audioSrc ? (
                              <audio controls src={audioSrc} className="w-full">
                                  Your browser does not support the audio element.
                              </audio>
                          ) : (
                              <p className="text-muted-foreground">No audio available for this file type.</p>
                          )}
                      </CardContent>
                  </Card>
              </TabsContent>
              <TabsContent value="metadata" className="flex-grow h-0 mt-4">
                  <Card className="h-full">
                      <CardHeader>
                          <CardTitle>Extracted Raw Metadata</CardTitle>
                          <CardDescription>The raw text block found in the binary file.</CardDescription>
                      </CardHeader>
                      <CardContent>
                          {fileContent.rawMetadata ? (
                              <pre className="text-xs bg-muted p-4 rounded-lg overflow-x-auto h-full max-h-full">
                                  <code>{fileContent.rawMetadata}</code>
                              </pre>
                          ) : (
                              <p className="text-muted-foreground">No parsable GUANO metadata was found in this file.</p>
                          )}
                      </CardContent>
                  </Card>
              </TabsContent>
          </Tabs>
      </CardContent>
    </Card>
  );
}
