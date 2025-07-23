
"use client";

import type { FileContent } from "@/lib/types";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { ScrollArea } from "./ui/scroll-area";
import { DataVisualizer } from "./data-visualizer";

export function FileContentViewer({ fileContent }: { fileContent: FileContent }) {
  const hasVisualization = !!fileContent.extractedData && fileContent.extractedData.length > 0;

  return (
    <ScrollArea className="h-full">
        {hasVisualization ? (
           <div className="space-y-4 p-1">
             <DataVisualizer data={fileContent.extractedData} />
             
             {fileContent.rawMetadata && (
                <Card>
                    <CardHeader>
                        <CardTitle>Extracted Metadata</CardTitle>
                         <CardDescription>
                            The raw text block used to generate the visualization above.
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
    </ScrollArea>
  );
}
