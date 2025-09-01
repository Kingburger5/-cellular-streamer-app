
"use client";

import type { FileContent } from "@/lib/types";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Badge } from "./ui/badge";
import { ServerCrash, FileSearch, Loader, Download } from "lucide-react";
import { DataVisualizer } from "./data-visualizer";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { SummaryViewer } from "./summary-viewer";
import { getDownloadUrlAction } from "@/app/actions";
import { useToast } from "@/hooks/use-toast";
import { useEffect, useState, useTransition } from "react";
import { Button } from "./ui/button";

interface FileDisplayProps {
  fileContent: FileContent | null;
  isLoading: boolean;
  error: string | null;
}

export function FileDisplay({ fileContent, isLoading, error }: FileDisplayProps) {
  const { toast } = useToast();
  const [audioUrl, setAudioUrl] = useState<string | null>(null);
  const [isAudioLoading, setIsAudioLoading] = useState<boolean>(false);
  const [isDownloading, startDownloadTransition] = useTransition();

  // Reset audio URL when the selected file changes
  useEffect(() => {
    setAudioUrl(null);
  }, [fileContent?.name]);

  const handleTabChange = async (tabValue: string) => {
    if (tabValue === "audio" && fileContent?.isBinary && !audioUrl && !isAudioLoading) {
      setIsAudioLoading(true);
      toast({ title: "Generating secure download link for audio..." });
      try {
        const result = await getDownloadUrlAction(fileContent.name);
        if ("url" in result) {
          setAudioUrl(result.url);
          toast({ title: "Audio ready for playback." });
        } else {
          toast({ title: "Error", description: result.error, variant: "destructive" });
        }
      } catch (e) {
        const message = e instanceof Error ? e.message : "An unknown error occurred.";
        toast({ title: "Failed to get audio URL", description: message, variant: "destructive" });
      } finally {
        setIsAudioLoading(false);
      }
    }
  };

  const handleDownloadFile = async () => {
    if (!fileContent) return;
    startDownloadTransition(async () => {
        toast({ title: `Preparing '${fileContent.name}' for download...` });
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
    });
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
  
  const isBinary = fileContent.isBinary;
  const hasRawMetadata = !!fileContent.rawMetadata;
  const hasData = !!fileContent.extractedData && fileContent.extractedData.length > 0;
  
  const isDataLoading = fileContent.rawMetadata ? !fileContent.extractedData : false;

  let defaultTab = "summary";
  if (hasData) {
      defaultTab = "visualization";
  } else if (!hasRawMetadata && isBinary) {
      defaultTab = "audio";
  } else if (!hasData && hasRawMetadata) {
      defaultTab = "metadata";
  }


  return (
    <Card className="h-full flex flex-col">
      <CardHeader>
        <div className="flex justify-between items-start gap-4">
            <div className="flex-1 min-w-0">
                <CardTitle className="font-headline flex items-center gap-2">
                  <span className="truncate">{fileContent.name}</span>
                  <Badge variant="outline">{fileContent.extension}</Badge>
                </CardTitle>
                <CardDescription>File from persistent storage. Select a tab to view details.</CardDescription>
            </div>
            <Button variant="outline" size="sm" onClick={handleDownloadFile} disabled={isDownloading}>
                <Download className="mr-2" />
                {isDownloading ? "Preparing..." : "Download"}
            </Button>
        </div>
      </CardHeader>
      <CardContent className="flex-grow h-0">
          <Tabs defaultValue={defaultTab} className="h-full flex flex-col" onValueChange={handleTabChange}>
              <TabsList className="shrink-0">
                  <TabsTrigger value="summary" disabled={!hasRawMetadata}>AI Summary</TabsTrigger>
                  <TabsTrigger value="visualization" disabled={!hasRawMetadata}>Data Visualization</TabsTrigger>
                  <TabsTrigger value="audio" disabled={!isBinary}>Audio Playback</TabsTrigger>
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
                          {isAudioLoading && (
                            <div className="flex items-center gap-2 text-muted-foreground">
                              <Loader className="w-4 h-4 animate-spin" />
                              <span>Preparing audio...</span>
                            </div>
                          )}
                          {audioUrl && (
                              <audio controls src={audioUrl} className="w-full" autoPlay>
                                  Your browser does not support the audio element.
                              </audio>
                          )}
                           {!audioUrl && !isAudioLoading && isBinary && (
                              <p className="text-muted-foreground">Audio will be loaded on demand. Click this tab again if needed.</p>
                           )}
                           {!isBinary && (
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
