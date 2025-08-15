
"use client";

import { useState, useTransition, useCallback, useEffect } from "react";
import { getLogsAction } from "@/app/actions";
import { Button } from "@/components/ui/button";
import { RefreshCw, Terminal } from "lucide-react";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Skeleton } from "@/components/ui/skeleton";

export function DebugLogViewer() {
    const [logs, setLogs] = useState("");
    const [isRefreshing, startRefreshTransition] = useTransition();

    const fetchLogs = useCallback(() => {
        startRefreshTransition(async () => {
            const logData = await getLogsAction();
            setLogs(logData);
        });
    }, []);

    // Fetch logs on initial component mount
    useEffect(() => {
        fetchLogs();
    }, [fetchLogs]);
    
    // Auto-refresh logs every 5 seconds
    useEffect(() => {
        const interval = setInterval(fetchLogs, 5000);
        return () => clearInterval(interval);
    }, [fetchLogs]);

    return (
        <Card className="h-full flex flex-col">
            <CardHeader>
                <div className="flex justify-between items-center">
                    <div>
                        <CardTitle className="flex items-center gap-2">
                           <Terminal /> Connection Log
                        </CardTitle>
                        <CardDescription>
                            Shows raw requests hitting the `/api/upload` endpoint. Updates automatically.
                        </CardDescription>
                    </div>
                    <Button variant="outline" size="sm" onClick={fetchLogs} disabled={isRefreshing}>
                        <RefreshCw className={`w-4 h-4 mr-2 ${isRefreshing ? 'animate-spin' : ''}`} />
                        Refresh
                    </Button>
                </div>
            </CardHeader>
            <CardContent className="flex-grow h-0">
                <ScrollArea className="h-full w-full bg-muted rounded-md">
                    <pre className="text-xs p-4">
                        {isRefreshing && !logs ? (
                           <Skeleton className="h-20 w-full" />
                        ) : (
                           <code>{logs}</code>
                        )}
                    </pre>
                </ScrollArea>
            </CardContent>
        </Card>
    );
}
