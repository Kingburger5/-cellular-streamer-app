
"use client";

import { useState, useEffect } from 'react';
import { getServerHealthAction } from '@/app/actions';
import type { ServerHealth } from '@/lib/types';
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Loader, CheckCircle, XCircle, AlertTriangle, ShieldCheck, ShieldAlert, Server } from 'lucide-react';
import { Alert, AlertDescription, AlertTitle } from '@/components/ui/alert';

function HealthCheckItem({ label, success, description }: { label: string, success: boolean, description?: string | null }) {
    return (
        <div className="flex items-start justify-between py-3 border-b last:border-b-0">
            <div className="flex items-center gap-3">
                 {success ? <CheckCircle className="w-5 h-5 text-green-500" /> : <XCircle className="w-5 h-5 text-destructive" />}
                <div>
                    <p className="font-medium">{label}</p>
                    {description && <p className="text-xs text-muted-foreground">{description}</p>}
                </div>
            </div>
        </div>
    )
}

export function ServerHealthViewer() {
    const [health, setHealth] = useState<ServerHealth | null>(null);
    const [isLoading, setIsLoading] = useState(false);

    const runHealthCheck = async () => {
        setIsLoading(true);
        const result = await getServerHealthAction();
        setHealth(result);
        setIsLoading(false);
    }

    useEffect(() => {
        runHealthCheck();
    }, []);

    const isOverallHealthy = health && health.canInitializeAdmin && health.hasClientEmail && health.canFetchAccessToken;

    return (
        <Card className="h-full flex flex-col">
            <CardHeader>
                <div className="flex justify-between items-center">
                    <div>
                        <CardTitle className="flex items-center gap-2">
                           <Server /> Server Health Check
                        </CardTitle>
                        <CardDescription>
                            Diagnostic checks for the server environment and authentication.
                        </CardDescription>
                    </div>
                    <Button onClick={runHealthCheck} disabled={isLoading}>
                        {isLoading ? <Loader className="mr-2 animate-spin"/> : null}
                        Re-run Checks
                    </Button>
                </div>
            </CardHeader>
            <CardContent>
                {isLoading && !health ? (
                    <div className="flex flex-col items-center justify-center h-full text-muted-foreground p-4 text-center">
                        <Loader className="w-12 h-12 mb-4 animate-spin" />
                        <h3 className="font-semibold">Running Diagnostics...</h3>
                    </div>
                ) : health && (
                    <>
                        <Card className={`mb-4 ${isOverallHealthy ? 'bg-green-50 border-green-200' : 'bg-destructive/10 border-destructive/20'}`}>
                             <CardHeader className="pb-2">
                                <div className="flex items-center gap-3">
                                    {isOverallHealthy ? <ShieldCheck className="w-8 h-8 text-green-600" /> : <ShieldAlert className="w-8 h-8 text-destructive" />}
                                    <div>
                                        <CardTitle>{isOverallHealthy ? 'Authentication Healthy' : 'Authentication Problem Detected'}</CardTitle>
                                        <CardDescription className={`${isOverallHealthy ? 'text-green-800' : 'text-destructive'}`}>
                                            {isOverallHealthy ? 'The server appears to be correctly authenticated and can communicate with Google services.' : 'The server is failing a critical authentication check. File processing will fail.'}
                                        </CardDescription>
                                    </div>
                                </div>
                            </CardHeader>
                        </Card>

                        <HealthCheckItem 
                            label="Admin SDK Initialized"
                            success={health.canInitializeAdmin}
                            description="Checks if the core Google Auth library can be initialized on the server."
                        />
                         <HealthCheckItem 
                            label="Project ID Detected"
                            success={health.hasProjectId}
                            description={`Checks for the project ID. Detected: ${health.projectId || 'None'}`}
                        />
                         <HealthCheckItem 
                            label="Client Email Found"
                            success={health.hasClientEmail}
                            description={`This is the critical credential. Detected: ${health.detectedClientEmail || 'None'}`}
                        />
                         <HealthCheckItem 
                            label="Private Key Found"
                            success={health.hasPrivateKey}
                            description="Checks if the private key for signing is present in the credentials."
                        />
                        <HealthCheckItem 
                            label="Access Token Generated"
                            success={health.canFetchAccessToken}
                            description="The final test. Can the server get a token to prove its identity?"
                        />

                        {health.accessTokenError && (
                             <Alert variant="destructive" className="mt-4">
                                <AlertTriangle className="h-4 w-4" />
                                <AlertTitle>Access Token Error</AlertTitle>
                                <AlertDescription>
                                   The server failed to get an access token. This is the root cause of the problem.
                                   <pre className="text-xs mt-2 p-2 bg-black/10 rounded-md overflow-x-auto"><code>{health.accessTokenError}</code></pre>
                                </AlertDescription>
                            </Alert>
                        )}
                    </>
                )}

            </CardContent>
        </Card>
    )

}
