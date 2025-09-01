
"use server";

import { adminStorage } from "@/lib/firebase-admin";
import { extractData } from "@/ai/flows/extract-data-flow";
import type { UploadedFile, FileContent, DataPoint } from "@/lib/types";


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

function findGuanoMetadataInChunk(chunk: Buffer): string | null {
    try {
        const guanoKeyword = Buffer.from("GUANO");
        const guanoIndex = chunk.indexOf(guanoKeyword);

        if (guanoIndex === -1) {
            console.log("DEBUG: GUANO keyword not found in the provided chunk.");
            return null;
        }

        const lengthOffset = guanoIndex - 4;
        if (lengthOffset < 0) {
            console.log("DEBUG: Not enough space for length before GUANO keyword.");
            return null;
        }

        const chunkLength = chunk.readUInt32LE(lengthOffset);
        
        if (chunkLength > chunk.length - guanoIndex || chunkLength <= 0) {
            console.log(`DEBUG: Invalid metadata chunk length (${chunkLength}).`);
            return null;
        }

        const metadataStart = guanoIndex;
        const metadataEnd = metadataStart + chunkLength;

        if (metadataEnd > chunk.length) {
            console.log(`DEBUG: Metadata chunk length (${chunkLength}) exceeds buffer size.`);
            return null;
        }

        const metadataContent = chunk.toString('utf-8', metadataStart, metadataEnd);
        console.log(`DEBUG: Successfully extracted metadata chunk of length ${chunkLength}.`);
        return metadataContent.trim();
    } catch (error) {
        console.error(`[SERVER_ERROR] Failed to process GUANO metadata from chunk:`, error);
        return null;
    }
}


export async function processFileAction(
  filename: string,
  fileChunkBase64: string | null // Receive the chunk from the client
): Promise<FileContent | { error: string }> {
    let rawMetadata: string | null = null;
    let extractedData: DataPoint[] | null = null;
    
    try {
        console.log(`[SERVER_INFO] Step 1 Started: Processing '${filename}' from client-provided chunk.`);
        
        const extension = filename.split('.').pop()?.toLowerCase() || '';

        // Only process chunk for binary files that might contain GUANO
        if (fileChunkBase64 && ['wav', 'mp3', 'ogg', 'zip', 'gz', 'bin'].includes(extension)) {
            const fileBuffer = Buffer.from(fileChunkBase64, 'base64');
            rawMetadata = findGuanoMetadataInChunk(fileBuffer);
             if (!rawMetadata) {
                 console.log(`[SERVER_INFO] Step 2 Incomplete: No GUANO metadata block found in binary file chunk '${filename}'.`);
            } else {
                 console.log(`[SERVER_INFO] Step 2 Success: Found GUANO metadata block from chunk.`);
            }
        } else if(fileChunkBase64) {
            // For text-based files, the chunk is the whole file content
            rawMetadata = Buffer.from(fileChunkBase64, 'base64').toString('utf-8');
            console.log(`[SERVER_INFO] Step 2 Success: Processed text file '${filename}'.`);
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
            }
        } else {
             console.log(`[SERVER_INFO] Step 3 Skipped: No raw metadata to send to AI for '${filename}'.`);
        }

        const isBinary = ['wav', 'mp3', 'ogg', 'zip', 'gz', 'bin'].includes(extension);
        const content = isBinary
            ? `data:application/octet-stream;base64,` // Placeholder, real content is not on the server
            : rawMetadata || ''; // For text files, content is the metadata

        return { 
            content, 
            extension, 
            name: filename, 
            isBinary,
            rawMetadata, 
            extractedData 
        };

    } catch(error: any) {
        console.error(`[SERVER_ERROR] Generic processing error for file ${filename}:`, error);
        return { error: `An unexpected error occurred during file processing. Code: ${error.code}. Check server logs.`};
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
        if (message.includes("client_email") || message.includes("credentials")) {
             return { error: `Server-side authentication failed: Could not find or use credentials to sign the URL. Please ensure your service account has 'Service Account Token Creator' role. Full error: ${message}` };
        }
        return { error: `Server-side download link generation failed: ${message}. Check logs and IAM permissions.` };
    }
}


// Logs are now viewed in the Firebase Console, not from a file.
export async function getLogsAction(): Promise<string> {
    return "Connection logs are now available in the Firebase Console under the 'Logs' tab for your App Hosting backend. Local file logging has been disabled.";
}
