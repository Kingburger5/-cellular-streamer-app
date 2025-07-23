
"use client";

import type { FileContent } from '@/lib/types';
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from '@/components/ui/card';
import { ScrollArea } from '@/components/ui/scroll-area';
import { DataVisualizer } from './data-visualizer';
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";

export function WaveFileViewer({ fileContent, isDataLoading }: { fileContent: FileContent, isDataLoading: boolean }) {
    const audioSrc = fileContent.isBinary ? fileContent.content : undefined;
    const hasRawMetadata = !!fileContent.rawMetadata;
    const hasData = !!fileContent.extractedData;
    const defaultTab = (hasData || isDataLoading) ? "visualization" : "metadata";

    return (
        <Tabs defaultValue={defaultTab} className="h-full flex flex-col">
            <TabsList className="shrink-0">
                <TabsTrigger value="visualization" disabled={!hasRawMetadata}>Data Visualization</TabsTrigger>
                <TabsTrigger value="audio" disabled={!audioSrc}>Audio Playback</TabsTrigger>
                <TabsTrigger value="metadata">Raw Metadata</TabsTrigger>
            </TabsList>
            <TabsContent value="visualization" className="flex-grow h-0">
                 <DataVisualizer data={fileContent.extractedData} isLoading={isDataLoading}/>
            </TabsContent>
            <TabsContent value="audio" className="flex-grow h-0">
                 <Card className="h-full">
                    <CardHeader>
                        <CardTitle>Audio Playback</CardTitle>
                    </CardHeader>
                    <CardContent>
                        {audioSrc && (
                            <audio controls src={audioSrc} className="w-full">
                                Your browser does not support the audio element.
                            </audio>
                        )}
                    </CardContent>
                </Card>
            </TabsContent>
            <TabsContent value="metadata" className="flex-grow h-0">
                 <Card className="h-full">
                    <CardHeader>
                        <CardTitle>Extracted Raw Metadata</CardTitle>
                        <CardDescription>The raw text block found in the binary file.</CardDescription>
                    </CardHeader>
                    <CardContent>
                        {fileContent.rawMetadata ? (
                             <pre className="text-xs bg-muted p-4 rounded-lg overflow-x-auto h-full">
                                <code>{fileContent.rawMetadata}</code>
                             </pre>
                        ) : (
                            <p className="text-muted-foreground">No parsable GUANO metadata was found in this file.</p>
                        )}
                    </CardContent>
                </Card>
            </TabsContent>
        </Tabs>
    );
}
