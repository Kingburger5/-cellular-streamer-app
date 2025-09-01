
"use server";

import { adminStorage } from "@/lib/firebase-admin";
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
        const guanoIndex = chunk.lastIndexOf(guanoKeyword);

        if (guanoIndex === -1) {
            return null;
        }

        const lengthOffset = guanoIndex - 4;
        if (lengthOffset < 0) {
            return null;
        }

        const chunkLength = chunk.readUInt32LE(lengthOffset);
        
        if (chunkLength > chunk.length - guanoIndex || chunkLength <= 0) {
            return null;
        }

        const metadataStart = guanoIndex;
        const metadataEnd = metadataStart + chunkLength;

        if (metadataEnd > chunk.length) {
            return null;
        }
        
        const rawMetadataSlice = chunk.subarray(metadataStart, metadataEnd);
        let metadataContent = '';
        for (let i = 0; i < rawMetadataSlice.length; i++) {
            const charCode = rawMetadataSlice[i];
            if ((charCode >= 32 && charCode <= 126) || charCode === 10 || charCode === 13 || charCode === 9) {
                 metadataContent += String.fromCharCode(charCode);
            }
        }
        
        return metadataContent.trim();
    } catch (error) {
        console.error(`[SERVER_ERROR] Failed to process GUANO metadata from chunk:`, error);
        return null;
    }
}


/**
 * Parses a GUANO metadata string into a structured DataPoint object.
 * This is a fast, reliable, rule-based parser that avoids AI unpredictability.
 */
function parseGuanoMetadata(metadataString: string, originalFilename: string): DataPoint | null {
  try {
    const fields = metadataString.split('|');
    const metadataMap = new Map<string, string>();

    fields.forEach(field => {
      const parts = field.split(':');
      if (parts.length > 1) {
        const key = parts[0].trim();
        const value = parts.slice(1).join(':').trim();
        metadataMap.set(key, value);
      }
    });

    let audioSettings: any = {};
    const audioSettingsString = metadataMap.get('Audio settings');
    if (audioSettingsString) {
      try {
        // The value is a JSON array string, e.g., '[{...}]'
        audioSettings = JSON.parse(audioSettingsString)[0];
      } catch (e) {
        console.error("Failed to parse 'Audio settings' JSON:", e);
      }
    }

    const locPosition = metadataMap.get('Loc Position')?.split(' ') || [];

    const dataPoint: DataPoint = {
      fileInformation: {
        originalFilename: metadataMap.get('Original Filename') || originalFilename,
        recordingDateTime: metadataMap.get('Timestamp'),
        recordingDurationSeconds: Number(metadataMap.get('Length')) || undefined,
        sampleRateHz: Number(metadataMap.get('Samplerate')) || undefined,
      },
      recorderDetails: {
        make: metadataMap.get('Make'),
        model: metadataMap.get('Model'),
        serialNumber: metadataMap.get('Serial'),
        firmwareVersion: metadataMap.get('Firmware Version'),
        gainSetting: audioSettings.gain,
      },
      locationEnvironmentalData: {
        latitude: locPosition[0] ? Number(locPosition[0]) : undefined,
        longitude: locPosition[1] ? Number(locPosition[1]) : undefined,
        temperatureCelsius: Number(metadataMap.get('Temperature Int')) || undefined,
      },
      triggerSettings: {
        windowSeconds: audioSettings['trig window'],
        maxLengthSeconds: audioSettings['trig max len'],
        minFrequencyHz: audioSettings['trig min freq'],
        maxFrequencyHz: audioSettings['trig max freq'],
        minDurationSeconds: audioSettings['trig min dur'],
        maxDurationSeconds: audioSettings['trig max dur'],
      },
    };
    
    return dataPoint;

  } catch (error) {
    console.error('[SERVER_ERROR] Failed to parse GUANO metadata string:', error);
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

        const CHUNK_SIZE = 1 * 1024 * 1024; // 1MB
        const start = Math.max(0, Number(metadata.size) - CHUNK_SIZE);
        const [fileBuffer] = await file.download({ start });

        if (isBinary) {
            rawMetadata = findGuanoMetadataInChunk(fileBuffer);
             if (!rawMetadata) {
                 console.log(`[SERVER_INFO] Step 2 Incomplete: No GUANO metadata block found in binary file chunk '${filename}'.`);
            } else {
                 console.log(`[SERVER_INFO] Step 2 Success: Found GUANO metadata block from chunk.`);
                 fileContentForClient = rawMetadata.replace(/\|/g, '\n'); 
            }
        } else {
            rawMetadata = fileBuffer.toString('utf-8');
            fileContentForClient = rawMetadata;
            console.log(`[SERVER_INFO] Step 2 Success: Processed text file '${filename}'.`);
        }

        if (rawMetadata) {
            // Use the fast, reliable parser instead of the AI
            const parsedData = parseGuanoMetadata(rawMetadata, filename);
            if (parsedData) {
                extractedData = [parsedData];
                console.log(`[SERVER_INFO] Step 3 Success: Manually parsed GUANO data.`);
            } else {
                console.log(`[SERVER_INFO] Step 3 Failed: Manual parser could not extract data from '${filename}'.`);
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
