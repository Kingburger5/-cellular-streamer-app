
"use server";

import { adminStorage } from "@/lib/firebase-admin";
import { extractData } from "@/ai/flows/extract-data-flow";
import { appendToSheet } from "@/ai/flows/append-to-sheet-flow";
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

function findReadableStrings(buffer: Buffer): string | null {
    const guanoKeyword = Buffer.from("GUANO");
    const guanoIndex = buffer.indexOf(guanoKeyword);

    if (guanoIndex === -1) {
        console.log("GUANO keyword not found in buffer.");
        return null;
    }

    // The 4 bytes before GUANO should contain the length of the metadata chunk.
    // It's a 32-bit little-endian unsigned integer.
    const lengthOffset = guanoIndex - 4;
    if (lengthOffset < 0) {
        console.log("Not enough space for length before GUANO keyword.");
        return null;
    }
    
    try {
        const chunkLength = buffer.readUInt32LE(lengthOffset);
        
        // The metadata starts right after the length.
        const metadataStart = guanoIndex;
        const metadataEnd = metadataStart + chunkLength;
        
        if (metadataEnd > buffer.length) {
            console.log("Metadata chunk length exceeds buffer size.");
            return null;
        }
        
        // Extract the metadata chunk as a string.
        const metadata = buffer.toString('utf-8', metadataStart, metadataEnd);
        return metadata.trim();

    } catch (e) {
        console.error("Error reading metadata length:", e);
        return null;
    }
}


export async function processFileAction(
  filename: string,
): Promise<FileContent | null> {
    try {
        const bucket = adminStorage.bucket();
        const file = bucket.file(`uploads/${filename}`);
        const [fileBuffer] = await file.download();

        const buffer = Buffer.from(fileBuffer);
        const extension = filename.split('.').pop()?.toLowerCase() || '';

        let content: string;
        let isBinary = false;
        let rawMetadata: string | null = null;
        let extractedData: DataPoint[] | null = null;

        // Broaden binary check for common audio/compressed formats
        if (['wav', 'mp3', 'ogg', 'zip', 'gz', 'bin'].includes(extension)) {
            isBinary = true;
            content = `data:application/octet-stream;base64,${buffer.toString('base64')}`;
            rawMetadata = findReadableStrings(buffer);
        } else {
            // Attempt to decode as UTF-8, fall back to raw if it fails
            try {
                content = buffer.toString("utf-8");
            } catch (e) {
                isBinary = true;
                content = `data:application/octet-stream;base64,${buffer.toString('base64')}`;
            }
            rawMetadata = content;
        }
        
        if (rawMetadata) {
            const aiResult = await extractData({ fileContent: rawMetadata, filename: filename });
             if (aiResult && aiResult.data.length > 0) {
                extractedData = aiResult.data;
            }
        }

        return { content, extension, name: filename, isBinary, rawMetadata, extractedData };

    } catch(error) {
        console.error(`Error processing file ${filename}:`, error);
        return null;
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
    return { error: message };
  }
}

export async function getDownloadUrlAction(filename: string): Promise<{ url: string } | { error: string }> {
    try {
        const bucket = adminStorage.bucket();
        const file = bucket.file(`uploads/${filename}`);
        const [url] = await file.getSignedUrl({
            action: 'read',
            expires: Date.now() + 15 * 60 * 1000, // 15 minutes
        });
        return { url };
    } catch (error) {
        const message = error instanceof Error ? error.message : "An unknown error occurred.";
        console.error(`Failed to get download URL for ${filename}:`, error);
        return { error: message };
    }
}


// Logs are now viewed in the Firebase Console, not from a file.
export async function getLogsAction(): Promise<string> {
    return "Connection logs are now available in the Firebase Console under the 'Logs' tab for your App Hosting backend. Local file logging has been disabled.";
}

// This new action will be called directly from the UI.
export async function syncToSheetAction(dataPoint: DataPoint, originalFilename: string) {
    try {
        const result = await appendToSheet({ dataPoint, originalFilename });
        console.log("Sync to sheet result:", result);
        return { success: true, message: result };
    } catch(error) {
        const message = error instanceof Error ? error.message : "An unknown error occurred.";
        console.error("Sync to sheet action failed:", error);
        return { success: false, error: message };
    }
}
