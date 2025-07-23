
"use client";

import type { FileContent } from "@/lib/types";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { ScrollArea } from "./ui/scroll-area";
import { DataVisualizer } from "./data-visualizer";

export function FileContentViewer({ fileContent }: { fileContent: FileContent }) {
  const hasVisualization = !!fileContent.extractedData && fileContent.extractedData.length > 0;

  return (
    <ScrollArea className="h-full">
      <div className="space-y-4 p-1">
        {hasVisualization ? (
           <DataVisualizer data={fileContent.extractedData} />
        ) : (
             <Card>
                <CardHeader>
                    <CardTitle>File Content</CardTitle>
                </CardHeader>
                <CardContent>
                     <pre className="text-sm whitespace-pre-wrap">{fileContent.content}</pre>
                </CardContent>
             </Card>
        )}
        
        {fileContent.rawMetadata && fileContent.isBinary && (
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
         )}
       </div>
    </ScrollArea>
  );
}
