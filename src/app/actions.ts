"use server";

import { adminStorage } from "@/lib/firebase-admin";
import { extractData } from "@/ai/flows/extract-data-flow";
import type { UploadedFile, FileContent, DataPoint } from "@/lib/types";

const BUCKET_NAME = "cellular-data-streamer.firebasestorage.app";

export async function getFilesAction(): Promise<UploadedFile[]> {
    try {
        const bucket = adminStorage.bucket(BUCKET_NAME);
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
        // GUANO spec says the metadata can be at the end of the file.
        // We search from the end of the chunk backwards.
        const guanoIndex = chunk.lastIndexOf(guanoKeyword);

        if (guanoIndex === -1) {
            console.log("DEBUG: GUANO keyword not found in the provided chunk.");
            return null;
        }

        // GUANO spec states the 4 bytes *before* the keyword is the metadata length (UInt32LE).
        const lengthOffset = guanoIndex - 4;
        if (lengthOffset < 0) {
            console.log("DEBUG: Not enough space for length before GUANO keyword.");
            return null;
        }

        const chunkLength = chunk.readUInt32LE(lengthOffset);
        
        // Basic validation on the parsed length
        if (chunkLength > chunk.length - guanoIndex || chunkLength <= 0) {
            console.log(`DEBUG: Invalid metadata chunk length read from file: ${chunkLength}. Buffer size from GUANO index: ${chunk.length - guanoIndex}`);
            return null;
        }

        const metadataStart = guanoIndex;
        const metadataEnd = metadataStart + chunkLength;

        if (metadataEnd > chunk.length) {
            console.log(`DEBUG: Metadata chunk length (${chunkLength}) exceeds buffer size.`);
            return null;
        }
        
        // The metadata is not necessarily UTF-8, but it's the most likely encoding for the text parts.
        // Let's be explicit and replace non-printable characters.
        const rawMetadataSlice = chunk.subarray(metadataStart, metadataEnd);
        let metadataContent = '';
        for (let i = 0; i < rawMetadataSlice.length; i++) {
            const charCode = rawMetadataSlice[i];
            // Allow printable ASCII characters (32-126) plus newline, carriage return, and tab
            if ((charCode >= 32 && charCode <= 126) || charCode === 10 || charCode === 13 || charCode === 9) {
                 metadataContent += String.fromCharCode(charCode);
            }
        }
        
        console.log(`DEBUG: Successfully extracted metadata chunk of length ${chunkLength}.`);
        return metadataContent.trim();
    } catch (error) {
        console.error(`[SERVER_ERROR] Failed to process GUANO metadata from chunk:`, error);
        return null;
    }
}


export async function processFileAction(
  filename: string
): Promise<FileContent | { error: string }> {
    let rawMetadata: string | null = null;
    let extractedData: DataPoint[] | null = null;
    let fileContentForClient: string = '';
    
    try {
        console.log(`[SERVER_INFO] Step 1 Started: Processing '${filename}' on the server.`);
        
        const bucket = adminStorage.bucket(BUCKET_NAME);
        const file = bucket.file(`uploads/${filename}`);
        const [metadata] = await file.getMetadata();

        const extension = filename.split('.').pop()?.toLowerCase() || '';
        const isBinary = ['wav', 'mp3', 'ogg', 'zip', 'gz', 'bin'].includes(extension);

        // Download a chunk of the file to look for metadata
        // For GUANO, metadata is often at the end. We download the last 1MB.
        const CHUNK_SIZE = 1 * 1024 * 1024; // 1MB
        const start = Math.max(0, Number(metadata.size) - CHUNK_SIZE);
        const [fileBuffer] = await file.download({ start });

        if (isBinary) {
            rawMetadata = findGuanoMetadataInChunk(fileBuffer);
             if (!rawMetadata) {
                 console.log(`[SERVER_INFO] Step 2 Incomplete: No GUANO metadata block found in binary file chunk '${filename}'.`);
            } else {
                 console.log(`[SERVER_INFO] Step 2 Success: Found GUANO metadata block from chunk.`);
                 fileContentForClient = rawMetadata.replace(/\|/g, '\n'); // Normalize for display
            }
        } else {
            // For text-based files, the buffer is the whole file content
            rawMetadata = fileBuffer.toString('utf-8');
            fileContentForClient = rawMetadata;
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

        return { 
            content: fileContentForClient, 
            extension, 
            name: filename, 
            isBinary,
            rawMetadata: fileContentForClient, 
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
    const bucket = adminStorage.bucket(BUCKET_NAME);
    await bucket.file(`uploads/${filename}`).delete();
    return { success: true };
  } catch (error) {
    const message = error instanceof Error ? error.message : "An unknown error occurred.";
    console.error(`Failed to delete ${filename}:`, error);
    return { error: `Server-side delete failed: ${message}. Check logs and IAM permissions.` };
  }
}

export async function getDownloadUrlAction(fileName: string) {
  const bucket = adminStorage.bucket(BUCKET_NAME);
  const file = bucket.file(`uploads/${fileName}`);

  // Generate a signed URL valid for 15 minutes that forces a download
  const [url] = await file.getSignedUrl({
    action: "read",
    expires: Date.now() + 15 * 60 * 1000,
    responseDisposition: `attachment; filename="${fileName}"`,
  });

  return url;
}


// Logs are now viewed in the Firebase Console, not from a file.
export async function getLogsAction(): Promise<string> {
    return "Connection logs are now available in the Firebase Console under the 'Logs' tab for your App Hosting backend. Local file logging has been disabled.";
}
