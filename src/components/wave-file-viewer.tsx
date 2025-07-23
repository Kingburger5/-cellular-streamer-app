"use client";

import type { FileContent } from '@/lib/types';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { ScrollArea } from '@/components/ui/scroll-area';
import { Music } from 'lucide-react';

export function WaveFileViewer({ fileContent }: { fileContent: FileContent }) {
    const audioSrc = `data:audio/wav;base64,${fileContent.content}`;
    return (
        <ScrollArea className="h-full">
            <div className="p-1">
                <Card>
                    <CardHeader>
                        <CardTitle className='flex items-center gap-2'><Music /> Audio Playback</CardTitle>
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
    );
}
