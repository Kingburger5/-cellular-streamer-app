
"use client";

import type { FileContent } from '@/lib/types';
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from '@/components/ui/card';
import { ScrollArea } from '@/components/ui/scroll-area';
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { DataVisualizer } from './data-visualizer';

export function WaveFileViewer({ fileContent }: { fileContent: FileContent }) {
    const audioSrc = `data:audio/wav;base64,${fileContent.content}`;
    const hasVisualization = !!fileContent.extractedData && fileContent.extractedData.length > 0;

    return (
        <Tabs defaultValue={hasVisualization ? "visualize" : "playback"} className="h-full flex flex-col">
            <TabsList>
                <TabsTrigger value="visualize" disabled={!hasVisualization}>Visualize</TabsTrigger>
                <TabsTrigger value="playback">Playback</TabsTrigger>
                <TabsTrigger value="metadata" disabled={!fileContent.rawMetadata}>Raw Metadata</TabsTrigger>
            </TabsList>
            
            <TabsContent value="visualize" className="flex-grow h-0">
                <DataVisualizer data={fileContent.extractedData} />
            </TabsContent>
            
            <TabsContent value="playback" className="flex-grow h-0">
                <ScrollArea className="h-full">
                    <div className="p-1">
                        <Card>
                            <CardHeader>
                                <CardTitle>Audio Playback</CardTitle>
                            </CardHeader>
                            <CardContent>
                                <div className="flex flex-col items-center justify-center p-4">
                                    <h3 className="text-lg font-medium mb-2">{fileContent.name.substring(fileContent.name.indexOf('-') + 1)}</h3>
                                    <audio controls src={audioSrc} className="w-full max-w-md">
                                        Your browser does not support the audio element.
                                    </audio>
                                </div>
                            </CardContent>
                        </Card>
                    </div>
                </ScrollArea>
            </TabsContent>

            <TabsContent value="metadata" className="flex-grow h-0">
                 <ScrollArea className="h-full">
                     <div className="p-1">
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
            </TabsContent>

        </Tabs>
    );
}
