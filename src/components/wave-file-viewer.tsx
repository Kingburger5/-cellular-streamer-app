
"use client";

import type { FileContent } from '@/lib/types';
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from '@/components/ui/card';
import { ScrollArea } from '@/components/ui/scroll-area';
import { DataVisualizer } from './data-visualizer';

export function WaveFileViewer({ fileContent }: { fileContent: FileContent }) {
    const audioSrc = fileContent.content; // It's already a data URI
    const hasVisualization = !!fileContent.extractedData && fileContent.extractedData.length > 0;

    return (
        <ScrollArea className="h-full">
            <div className="space-y-4 p-1">
                 <Card>
                    <CardHeader>
                        <CardTitle>Audio Playback</CardTitle>
                    </CardHeader>
                    <CardContent>
                        <audio controls src={audioSrc} className="w-full">
                            Your browser does not support the audio element.
                        </audio>
                    </CardContent>
                </Card>

                {hasVisualization ? (
                    <DataVisualizer data={fileContent.extractedData} />
                ) : (
                    <Card>
                        <CardHeader>
                            <CardTitle>Metadata</CardTitle>
                        </CardHeader>
                        <CardContent>
                            {fileContent.rawMetadata ? (
                                 <pre className="text-xs bg-muted p-4 rounded-lg overflow-x-auto">
                                    <code>{fileContent.rawMetadata}</code>
                                 </pre>
                            ) : (
                                <p className="text-muted-foreground">No parsable GUANO metadata was found in this file.</p>
                            )}
                        </CardContent>
                    </Card>
                )}
            </div>
        </ScrollArea>
    );
}
