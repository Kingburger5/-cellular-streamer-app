
"use server";

import { getAdminStorage } from "@/lib/firebase-admin";
import type { UploadedFile, FileContent, DataPoint } from "@/lib/types";
import { extractData } from "@/ai/flows/extract-data-flow";

const BUCKET_NAME = "cellular-data-streamer.firebasestorage.app";

export async function getFilesAction(): Promise<UploadedFile[]> {
    try {
        const adminStorage = await getAdminStorage();
        const bucket = adminStorage.bucket(BUCKET_NAME);
        const [files] = await bucket.getFiles({ prefix: "uploads/"});

        const fileDetails = await Promise.all(
            files.map(async (file) => {
                // Skip objects that represent directories
                if (file.name.endsWith('/')) {
                    return null;
                }
                const [metadata] = await file.getMetadata();
                // Ensure name and timeCreated are defined before creating the object
                if (metadata.name && metadata.timeCreated) {
                    return {
                        name: metadata.name.replace('uploads/', ''), // Return just the filename
                        size: Number(metadata.size) || 0,
                        uploadDate: new Date(metadata.timeCreated),
                    };
                }
                return null;
            })
        );
        
        // Filter out any null results and sort the valid files
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
        const guanoIndex = chunk.lastIndexOf(guanoKeyword);

        if (guanoIndex === -1) {
            return null;
        }

        const lengthOffset = guanoIndex - 4;
        if (lengthOffset < 0) {
            return null;
        }

        // The length field in GUANO is the length of the metadata *following* the length field and keyword.
        // The total size of the block from the start of the 'GUANO' keyword is chunkLength.
        const chunkLength = chunk.readUInt32LE(lengthOffset);
        
        if (chunkLength > chunk.length - guanoIndex || chunkLength <= 0) {
            console.log(`[SERVER_WARNING] Invalid GUANO length field. Length: ${chunkLength}, Buffer available: ${chunk.length - guanoIndex}`);
            return null;
        }

        const metadataStart = guanoIndex;
        const metadataEnd = metadataStart + chunkLength;

        if (metadataEnd > chunk.length) {
            console.log(`[SERVER_WARNING] GUANO metadata block exceeds buffer length.`);
            return null;
        }
        
        const rawMetadataSlice = chunk.subarray(metadataStart, metadataEnd);
        // GUANO metadata is specified to be UTF-8
        let metadataContent = new TextDecoder('utf-8').decode(rawMetadataSlice);
        
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
        
        const adminStorage = await getAdminStorage();
        const bucket = adminStorage.bucket(BUCKET_NAME);
        const file = bucket.file(`uploads/${filename}`);

        const [exists] = await file.exists();
        if (!exists) {
            return { error: `File not found in storage: ${filename}` };
        }

        const [fileBuffer] = await file.download();
        console.log(`[SERVER_INFO] Successfully downloaded ${filename} from Firebase Storage.`);

        const extension = filename.split('.').pop()?.toLowerCase() || '';
        const isBinary = ['wav', 'mp3', 'ogg', 'zip', 'gz', 'bin'].includes(extension);

        if (isBinary) {
            rawMetadata = findGuanoMetadataInChunk(fileBuffer);
             if (!rawMetadata) {
                 console.log(`[SERVER_INFO] Step 2 Incomplete: No GUANO metadata block found in binary file '${filename}'.`);
                 // Fallback for client display if no GUANO
                 fileContentForClient = "No GUANO metadata found in this binary file.";
            } else {
                 console.log(`[SERVER_INFO] Step 2 Success: Found GUANO metadata block.`);
                 fileContentForClient = rawMetadata;
            }
        } else {
            // For text files, the whole content is the metadata
            rawMetadata = fileBuffer.toString('utf-8');
            fileContentForClient = rawMetadata;
            console.log(`[SERVER_INFO] Step 2 Success: Processed text file '${filename}'.`);
        }

        if (rawMetadata) {
            console.log(`[SERVER_INFO] Step 3 Started: AI processing of '${filename}' metadata.`);
            const aiResult = await extractData({ fileContent: rawMetadata, filename });
            if (aiResult) {
                extractedData = aiResult.data;
                console.log(`[SERVER_INFO] Step 3 Success: AI successfully extracted data.`);
            } else {
                 console.log(`[SERVER_INFO] Step 3 Failed: AI could not extract data from '${filename}'.`);
            }
        } else {
             console.log(`[SERVER_INFO] Step 3 Skipped: No raw metadata to process for '${filename}'.`);
        }

        return { 
            content: fileContentForClient, 
            extension, 
            name: filename, 
            isBinary,
            rawMetadata: rawMetadata, 
            extractedData 
        };

    } catch(error: any) {
        console.error(`[SERVER_ERROR] Generic processing error for file ${filename}:`, error);
        return { error: `An unexpected error occurred during file processing. Code: ${error.code || 'N/A'}. Check server logs.`};
    }
}

export async function deleteFileAction(
  filename: string
): Promise<{ success: true } | { error: string }> {
  try {
    const adminStorage = await getAdminStorage();
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
  const adminStorage = await getAdminStorage();
  const bucket = adminStorage.bucket(BUCKET_NAME);
  const file = bucket.file(`uploads/${fileName}`);

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
