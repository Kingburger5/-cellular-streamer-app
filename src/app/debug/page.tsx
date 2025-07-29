
"use client";

import { useState, useEffect } from 'react';
import { database } from '@/lib/firebase';
import { ref, onValue, off } from 'firebase/database';
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from '@/components/ui/card';
import { ScrollArea } from '@/components/ui/scroll-area';
import { Badge } from '@/components/ui/badge';
import { Activity, AlertTriangle, CheckCircle2 } from 'lucide-react';
import { FormattedDate } from '@/components/formatted-date';

interface UploadStatus {
    status: string;
    error: string | null;
    lastUpdated: string;
}

interface Uploads {
    [key: string]: UploadStatus;
}

export default function DebugPage() {
    const [uploads, setUploads] = useState<Uploads>({});

    useEffect(() => {
        const uploadsRef = ref(database, 'uploads');
        const listener = onValue(uploadsRef, (snapshot) => {
            const data = snapshot.val();
            if (data) {
                setUploads(data);
            } else {
                setUploads({});
            }
        });

        // Detach listener on cleanup
        return () => {
            off(uploadsRef, 'value', listener);
        };
    }, []);

    const getStatusVariant = (status: UploadStatus) => {
        if (status.error) return "destructive";
        if (status.status.toLowerCase().includes('complete')) return "default";
        return "secondary";
    };

    const getStatusIcon = (status: UploadStatus) => {
         if (status.error) return <AlertTriangle className="text-destructive" />;
        if (status.status.toLowerCase().includes('complete')) return <CheckCircle2 className="text-green-500" />;
        return <Activity className="animate-pulse" />;
    }

    const sortedUploadKeys = Object.keys(uploads).sort((a, b) => 
        new Date(uploads[b].lastUpdated).getTime() - new Date(uploads[a].lastUpdated).getTime()
    );


    return (
        <div className="h-screen bg-muted/40 p-4">
            <Card className="max-w-4xl mx-auto">
                <CardHeader>
                    <CardTitle>Live Upload Status</CardTitle>
                    <CardDescription>
                        This page shows the real-time status of file uploads being received by the server.
                    </CardDescription>
                </CardHeader>
                <CardContent>
                    <ScrollArea className="h-[70vh] border rounded-lg p-4">
                        {sortedUploadKeys.length === 0 ? (
                             <div className="flex flex-col items-center justify-center h-full text-muted-foreground">
                                <p>No uploads detected yet.</p>
                                <p className="text-sm">Waiting for data from a device...</p>
                            </div>
                        ) : (
                            <div className="space-y-4">
                                {sortedUploadKeys.map(key => (
                                    <div key={key} className="p-4 rounded-md shadow-sm border flex items-start gap-4">
                                       <div className="pt-1">
                                            {getStatusIcon(uploads[key])}
                                       </div>
                                       <div className="flex-grow">
                                            <h3 className="font-semibold text-sm break-all">{key}</h3>
                                            <p className={`text-sm ${uploads[key].error ? 'text-destructive' : 'text-muted-foreground'}`}>
                                                {uploads[key].error ? `Error: ${uploads[key].error}` : uploads[key].status}
                                            </p>
                                            <p className="text-xs text-muted-foreground/80 mt-1">
                                               Last Updated: <FormattedDate date={new Date(uploads[key].lastUpdated)} />
                                            </p>
                                        </div>
                                        <div>
                                            <Badge variant={getStatusVariant(uploads[key])}>
                                                {uploads[key].error ? 'Failed' : uploads[key].status.toLowerCase().includes('complete') ? 'Complete' : 'In Progress'}
                                            </Badge>
                                        </div>
                                    </div>
                                ))}
                            </div>
                        )}
                    </ScrollArea>
                </CardContent>
            </Card>
        </div>
    );
}

