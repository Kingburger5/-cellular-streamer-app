
"use client";

import type { FileContent } from "@/lib/types";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { ScrollArea } from "./ui/scroll-area";

export function FileContentViewer({ fileContent }: { fileContent: FileContent }) {
  const isAudio = ['.wav', '.mp3', 'ogg'].includes(fileContent.extension);

  if (isAudio) {
     return (
        <ScrollArea className="h-full">
            <div className="space-y-4 p-1">
                <Card>
                    <CardHeader>
                        <CardTitle>Audio File</CardTitle>
                    </CardHeader>
                    <CardContent>
                        <audio controls src={fileContent.content} className="w-full">
                            Your browser does not support the audio element.
                        </audio>
                    </CardContent>
                </Card>
                <Card>
                    <CardHeader>
                        <CardTitle>Extracted Raw Metadata</CardTitle>
                        <CardDescription>
                            The raw text block found in the binary file.
                        </CardDescription>
                    </CardHeader>
                    <CardContent>
                        <pre className="text-xs bg-muted p-4 rounded-lg overflow-x-auto">
                            <code>
                                {fileContent.rawMetadata 
                                    ? fileContent.rawMetadata 
                                    : "No parsable GUANO metadata was found in this file."}
                            </code>
                        </pre>
                    </CardContent>
                </Card>
            </div>
        </ScrollArea>
     )
  }
  
  // Fallback for non-audio text-based files like JSON, CSV, TXT
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
