
"use client";

import type { FileContent } from "@/lib/types";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { DataVisualizer } from "./data-visualizer";
import { ScrollArea } from "./ui/scroll-area";

export function FileContentViewer({ fileContent }: { fileContent: FileContent }) {
  const hasVisualization = !!fileContent.extractedData && fileContent.extractedData.length > 0;
  const isAudio = ['.wav', '.mp3', 'ogg'].includes(fileContent.extension);

  if (hasVisualization) {
    return (
        <ScrollArea className="h-full">
            <div className="space-y-4 p-1">
                <DataVisualizer data={fileContent.extractedData} />
                 <Card>
                    <CardHeader>
                        <CardTitle>Extracted Raw Metadata</CardTitle>
                        <CardDescription>
                            The raw text block found in the binary file.
                        </CardDescription>
                    </CardHeader>
                    <CardContent>
                        <pre className="text-xs bg-muted p-4 rounded-lg overflow-x-auto">
                            <code>{fileContent.rawMetadata}</code>
                        </pre>
                    </CardContent>
                </Card>
            </div>
        </ScrollArea>
    )
  }

  if (isAudio && fileContent.isBinary) {
     return (
       <Card className="h-full">
        <CardHeader>
            <CardTitle>Audio File</CardTitle>
            <CardDescription>No parsable GUANO metadata was found in this file.</CardDescription>
        </CardHeader>
        <CardContent>
            <div className="flex flex-col items-center justify-center p-4">
                <h3 className="text-lg font-medium mb-2">{fileContent.name.substring(fileContent.name.indexOf('-') + 1)}</h3>
                <audio controls src={`data:audio/wav;base64,${fileContent.content}`} className="w-full max-w-md">
                    Your browser does not support the audio element.
                </audio>
            </div>
        </CardContent>
     </Card>
     )
  }
  
  // Fallback for non-audio files or files without metadata
  return (
     <Card className="h-full">
        <CardHeader>
            <CardTitle>Raw File Content</CardTitle>
        </CardHeader>
        <CardContent>
             <pre className="text-sm whitespace-pre-wrap">{fileContent.content}</pre>
        </CardContent>
     </Card>
  );
}
