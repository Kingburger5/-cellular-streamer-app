"use client";

import type { FileContent } from '@/lib/types';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { ScrollArea } from '@/components/ui/scroll-area';
import { Music, FileWarning } from 'lucide-react';
import { DataVisualizer } from './data-visualizer';

function AudioPlayer({ fileContent }: { fileContent: FileContent }) {
    const audioSrc = `data:audio/wav;base64,${fileContent.content}`;
    return (
        <div className="flex flex-col items-center justify-center p-4">
            <Music className="w-16 h-16 text-primary mb-4" />
            <h3 className="text-lg font-medium mb-2">{fileContent.name.substring(fileContent.name.indexOf('-') + 1)}</h3>
            <audio controls src={audioSrc} className="w-full max-w-md">
                Your browser does not support the audio element.
            </audio>
        </div>
    );
}

export function WaveFileViewer({ fileContent }: { fileContent: FileContent }) {
    return (
        <ScrollArea className="h-full">
            <div className="p-1 space-y-4">
                <Card>
                    <CardHeader>
                        <CardTitle>Audio Playback</CardTitle>
                    </CardHeader>
                    <CardContent>
                        <AudioPlayer fileContent={fileContent} />
                    </CardContent>
                </Card>
                
                <Card>
                    <CardHeader>
                        <CardTitle>Extracted Metadata</CardTitle>
                    </CardHeader>
                    <CardContent className="h-auto">
                        {fileContent.extractedData && fileContent.extractedData.length > 0 ? (
                            <div className='h-[70vh]'>
                               <DataVisualizer data={fileContent.extractedData} />
                            </div>
                        ) : (
                             <div className="flex flex-col items-center justify-center h-48 text-muted-foreground p-4 text-center">
                                <FileWarning className="w-12 h-12 mb-4" />
                                <h3 className="font-semibold">No Metadata Found</h3>
                                <p className="text-sm">Could not extract any structured data from this file.</p>
                            </div>
                        )}
                    </CardContent>
                </Card>
            </div>
        </ScrollArea>
    )
}
