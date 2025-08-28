
"use client";

import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Terminal, Info } from "lucide-react";
import { Button } from "./ui/button";

export function DebugLogViewer() {
    
    const openLogsInNewTab = () => {
        // This URL is constructed based on standard Google Cloud Logging for App Hosting.
        // It assumes the project ID and backend ID from the firebase.json and apphosting.yaml files.
        const projectID = "cellular-data-streamer";
        const backendID = "my-app";
        const consoleUrl = `https://console.cloud.google.com/logs/viewer?project=${projectID}&resource=apphosting.googleapis.com%2FBackend%2F${backendID}`;
        window.open(consoleUrl, '_blank');
    }

    return (
        <Card className="h-full flex flex-col">
            <CardHeader>
                <div className="flex justify-between items-center">
                    <div>
                        <CardTitle className="flex items-center gap-2">
                           <Terminal /> Connection Log
                        </CardTitle>
                        <CardDescription>
                            Real-time request logs are available in Google Cloud Logging.
                        </CardDescription>
                    </div>
                    <Button onClick={openLogsInNewTab}>
                        Open Cloud Logs
                    </Button>
                </div>
            </CardHeader>
            <CardContent className="flex-grow h-0">
                <div className="h-full w-full bg-muted rounded-md p-6 flex flex-col items-center justify-center text-center">
                   <Info className="w-12 h-12 mb-4 text-muted-foreground" />
                   <h3 className="font-semibold">View Logs in Google Cloud</h3>
                   <p className="text-sm text-muted-foreground max-w-md">
                    To see live logs from your server, including file processing and potential errors, click the button above to go to the Google Cloud Logging page for this backend. Local file logging has been disabled.
                   </p>
                </div>
            </CardContent>
        </Card>
    );
}
