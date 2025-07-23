
"use client";

import type { FileContent } from "@/lib/types";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { DataVisualizer } from "./data-visualizer";
import { ScrollArea } from "./ui/scroll-area";
import { WaveFileViewer } from "./wave-file-viewer";

export function FileContentViewer({ fileContent }: { fileContent: FileContent }) {
  const isAudio = ['.wav', '.mp3', 'ogg'].includes(fileContent.extension);

  if (isAudio) {
    return <WaveFileViewer fileContent={fileContent} />;
  }
  
  // Fallback for non-audio text-based files like JSON, CSV, TXT
  // which might also contain parsable data.
  const hasVisualization = !!fileContent.extractedData && fileContent.extractedData.length > 0;
  if (hasVisualization) {
    return (
        <ScrollArea className="h-full">
            <div className="space-y-4 p-1">
                <DataVisualizer data={fileContent.extractedData} />
                 <Card>
                    <CardHeader>
                        <CardTitle>Raw File Content</CardTitle>
                        <CardDescription>
                            The raw text content from the file used for visualization.
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
  
  // Fallback for files with no visualizable data
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
