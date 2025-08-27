
"use client";

import type { FileContent } from "@/lib/types";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { WaveFileViewer } from "./wave-file-viewer";
import { Badge } from "./ui/badge";
import { ServerCrash, FileSearch, Loader, Download } from "lucide-react";
import { Button } from "./ui/button";
import { getDownloadUrlAction } from "@/app/actions";
import { useToast } from "@/hooks/use-toast";

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
        <h3 className="font-semibold">Loading File Data...</h3>
        <p className="text-sm">Please wait while the file is being retrieved and analyzed.</p>
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
        <p className="text-muted-foreground">Select a file from the list to view its contents and analysis.</p>
      </Card>
    );
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
                <CardDescription>File from persistent storage.</CardDescription>
            </div>
            <Button variant="outline" size="sm" onClick={handleDownload}>
                <Download className="mr-2" />
                Download
            </Button>
        </div>
      </CardHeader>
      <CardContent className="flex-grow h-0">
          <WaveFileViewer fileContent={fileContent} />
      </CardContent>
    </Card>
  );
}
