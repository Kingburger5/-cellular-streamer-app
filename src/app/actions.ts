
"use server";

import { adminStorage } from "@/lib/firebase-admin";
import { extractData } from "@/ai/flows/extract-data-flow";
import type { UploadedFile, FileContent, DataPoint, ServerHealth } from "@/lib/types";
import { GoogleAuth } from 'google-auth-library';


export async function getServerHealthAction(): Promise<ServerHealth> {
    const health: ServerHealth = {
        canInitializeAdmin: false,
        hasProjectId: false,
        hasClientEmail: false,
        hasPrivateKey: false,
        canFetchAccessToken: false,
        accessTokenError: null,
        projectId: null,
        detectedClientEmail: null,
    };

    try {
        const auth = new GoogleAuth();
        const client = await auth.getClient();
        
        health.canInitializeAdmin = true; // If we get this far, the basic lib is working.

        // @ts-ignore
        const credentials = client.credentials;
        
        if (credentials) {
            if (credentials.project_id) {
                health.hasProjectId = true;
                health.projectId = credentials.project_id;
            }
            if (credentials.client_email) {
                health.hasClientEmail = true;
                health.detectedClientEmail = credentials.client_email;
            }
            if (credentials.private_key) {
                health.hasPrivateKey = true;
            }
        }
        
        try {
            const token = await auth.getAccessToken();
            if (token) {
                health.canFetchAccessToken = true;
            }
        } catch (tokenError: any) {
            health.accessTokenError = tokenError.message;
        }

    } catch (error: any) {
        health.accessTokenError = `Failed to initialize GoogleAuth client: ${error.message}`;
    }

    return health;
}


// This function is no longer called by the main view but is kept for potential future server-side needs.
export async function getFilesAction(): Promise<UploadedFile[]> {
    try {
        const bucket = adminStorage.bucket();
        const [files] = await bucket.getFiles({ prefix: "uploads/"});

        const fileDetails = await Promise.all(
            files.map(async (file) => {
                if (file.name.endsWith('/')) { // Skip directories
                    return null;
                }
                const [metadata] = await file.getMetadata();
                return {
                    name: metadata.name.replace('uploads/', ''), // Return just the filename
                    size: Number(metadata.size),
                    uploadDate: new Date(metadata.timeCreated),
                };
            })
        );
        
        return fileDetails
            .filter((file): file is UploadedFile => file !== null)
            .sort((a, b) => b.uploadDate.getTime() - a.uploadDate.getTime());

    } catch (error) {
        console.error("[Server] Error fetching files from Firebase Storage:", error);
        throw new Error("Could not fetch files from storage. Please ensure your Firebase project is configured correctly and security rules are set.");
    }
}

function findReadableStrings(buffer: Buffer): string | null {
    const guanoKeyword = Buffer.from("GUANO");
    const guanoIndex = buffer.indexOf(guanoKeyword);

    if (guanoIndex === -1) {
        console.log("DEBUG: GUANO keyword not found in buffer.");
        return null;
    }

    // The 4 bytes before GUANO should contain the length of the metadata chunk.
    // It's a 32-bit little-endian unsigned integer.
    const lengthOffset = guanoIndex - 4;
    if (lengthOffset < 0) {
        console.log("DEBUG: Not enough space for length before GUANO keyword.");
        return null;
    }
    
    try {
        const chunkLength = buffer.readUInt32LE(lengthOffset);
        
        // The metadata starts right after the length.
        const metadataStart = guanoIndex;
        const metadataEnd = metadataStart + chunkLength;
        
        if (metadataEnd > buffer.length) {
            console.log(`DEBUG: Metadata chunk length (${chunkLength}) exceeds buffer size (${buffer.length}).`);
            return null;
        }
        
        // Extract the metadata chunk as a string.
        const metadata = buffer.toString('utf-8', metadataStart, metadataEnd);
        console.log(`DEBUG: Successfully extracted metadata chunk of length ${chunkLength}.`);
        return metadata.trim();

    } catch (e) {
        console.error("DEBUG: Error reading metadata length from buffer:", e);
        return null;
    }
}


export async function processFileAction(
  filename: string,
): Promise<FileContent | { error: string }> {
    let fileBuffer: Buffer;
    try {
        console.log(`[SERVER_INFO] Step 1 Started: Downloading '${filename}' from Firebase Storage.`);
        const bucket = adminStorage.bucket();
        const file = bucket.file(`uploads/${filename}`);
        const [downloadedBuffer] = await file.download();
        fileBuffer = downloadedBuffer;
        console.log(`[SERVER_INFO] Step 1 Success: Successfully downloaded ${fileBuffer.byteLength} bytes.`);
    } catch (error: any) {
        console.error(`[SERVER_ERROR] Step 1 Failed: Could not download file '${filename}' from Firebase Storage. Full Error:`, error);
        return { error: `Failed to download file from storage. See server logs for details. Code: ${error.code}` };
    }

    try {
        console.log(`[SERVER_INFO] Step 2 Started: Processing content for '${filename}'.`);
        const buffer = Buffer.from(fileBuffer);
        const extension = filename.split('.').pop()?.toLowerCase() || '';

        let content: string;
        let isBinary = false;
        let rawMetadata: string | null = null;
        let extractedData: DataPoint[] | null = null;

        if (['wav', 'mp3', 'ogg', 'zip', 'gz', 'bin'].includes(extension)) {
            isBinary = true;
            content = `data:application/octet-stream;base64,${buffer.toString('base64')}`;
            rawMetadata = findReadableStrings(buffer);
            if (!rawMetadata) {
                 console.log(`[SERVER_INFO] Step 2 Incomplete: No GUANO metadata block found in binary file '${filename}'.`);
            } else {
                 console.log(`[SERVER_INFO] Step 2 Success: Found GUANO metadata block.`);
            }
        } else {
             try {
                content = buffer.toString("utf-8");
                rawMetadata = content; // For text files, the whole content is metadata.
                 console.log(`[SERVER_INFO] Step 2 Success: Processed text file '${filename}'.`);
            } catch (e) {
                isBinary = true;
                content = `data:application/octet-stream;base64,${buffer.toString('base64')}`;
                 console.log(`[SERVER_INFO] Step 2 Incomplete: Could not decode as UTF-8, treating as binary.`);
            }
        }
        
        if (rawMetadata) {
            try {
                console.log(`[SERVER_INFO] Step 3 Started: Calling AI to extract data from '${filename}'.`);
                const aiResult = await extractData({ fileContent: rawMetadata, filename: filename });
                if (aiResult && aiResult.data.length > 0) {
                    extractedData = aiResult.data;
                    console.log(`[SERVER_INFO] Step 3 Success: AI extracted ${aiResult.data.length} data point(s).`);
                } else {
                    console.log(`[SERVER_INFO] Step 3 Incomplete: AI returned no data points from '${filename}'.`);
                }
            } catch (aiError) {
                 console.error(`[SERVER_ERROR] Step 3 Failed: AI data extraction failed for '${filename}'.`, aiError);
                 // Still return the file content, just without the extracted data
            }
        } else {
             console.log(`[SERVER_INFO] Step 3 Skipped: No raw metadata to send to AI for '${filename}'.`);
        }

        return { content, extension, name: filename, isBinary, rawMetadata, extractedData };

    } catch(error) {
        console.error(`[SERVER_ERROR] Generic processing error for file ${filename}:`, error);
        return { error: 'An unexpected error occurred during file processing. Check server logs.'};
    }
}

export async function deleteFileAction(
  filename: string
): Promise<{ success: true } | { error: string }> {
  try {
    const bucket = adminStorage.bucket();
    await bucket.file(`uploads/${filename}`).delete();
    return { success: true };
  } catch (error) {
    const message = error instanceof Error ? error.message : "An unknown error occurred.";
    console.error(`Failed to delete ${filename}:`, error);
    return { error: `Server-side delete failed: ${message}. Check logs and IAM permissions.` };
  }
}

export async function getDownloadUrlAction(filename: string): Promise<{ url: string } | { error: string }> {
    try {
        console.log(`[SERVER_INFO] Attempting to get signed URL for '${filename}'.`);
        const bucket = adminStorage.bucket();
        const file = bucket.file(`uploads/${filename}`);
        const [url] = await file.getSignedUrl({
            action: 'read',
            expires: Date.now() + 15 * 60 * 1000, // 15 minutes
        });
        console.log(`[SERVER_INFO] Successfully generated signed URL for '${filename}'.`);
        return { url };
    } catch (error: any) {
        const message = error instanceof Error ? error.message : "An unknown error occurred.";
        console.error(`[SERVER_ERROR] Failed to get download URL for ${filename}:`, error);
        // Provide a more specific error message based on the likely cause.
        if (message.includes("client_email")) {
             return { error: `Server-side authentication failed: Could not find credentials to sign the URL. Please check server logs and IAM permissions for the App Hosting service account. Full error: ${message}` };
        }
        return { error: `Server-side download link generation failed: ${message}. Check logs and IAM permissions.` };
    }
}


// Logs are now viewed in the Firebase Console, not from a file.
export async function getLogsAction(): Promise<string> {
    return "Connection logs are now available in the Firebase Console under the 'Logs' tab for your App Hosting backend. Local file logging has been disabled.";
}
