
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";
import os from "os";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");
// Use the OS's designated temporary directory, which is guaranteed to be writable.
const TMP_DIR = path.join(os.tmpdir(), "cellular-uploads");

// Helper to ensure directories exist
async function ensureDirsExist() {
  try {
    await fs.mkdir(UPLOAD_DIR, { recursive: true });
    await fs.mkdir(TMP_DIR, { recursive: true });
  } catch (error: any) {
    // This is a critical failure, log it and re-throw
    console.error("[SERVER] Fatal: Could not create server directories.", {
        uploadDir: UPLOAD_DIR,
        tmpDir: TMP_DIR,
        error: error.message,
    });
    throw new Error("Could not create server directories.");
  }
}

function parseUserDataHeader(header: string | null): Record<string, string> {
    const data: Record<string, string> = {};
    if (!header) {
        return data;
    }
    const pairs = header.split(';').map(s => s.trim());
    for (const pair of pairs) {
        // Split only on the first colon to handle potential colons in values
        const parts = pair.split(/:(.*)/s);
        if (parts.length === 2) {
            // Lowercase the key for case-insensitive matching
            const key = parts[0].trim().toLowerCase();
            const value = parts[1].trim();
            data[key] = value;
        }
    }
    return data;
}

export async function POST(request: NextRequest) {
  try {
    await ensureDirsExist();
    
    // The SIM7600G AT+HTTPPARA="USERDATA" sends metadata as a single header named "x-userdata".
    // Next.js automatically lowercases all incoming header names.
    const userData = parseUserDataHeader(request.headers.get("x-userdata"));

    const fileId = userData['x-file-id'];
    const chunkIndexStr = userData['x-chunk-index'];
    const totalChunksStr = userData['x-total-chunks'];
    const originalFilenameUnsafe = userData['x-original-filename'];

    if (!fileId || !chunkIndexStr || !totalChunksStr || !originalFilenameUnsafe) {
        console.error("[SERVER] Missing required headers", { userData });
        return NextResponse.json({ error: "Missing required x-file-id, x-chunk-index, x-total-chunks, or x-original-filename header." }, { status: 400 });
    }
    
    const chunkIndex = parseInt(chunkIndexStr);
    const totalChunks = parseInt(totalChunksStr);

    // Sanitize the filename to remove characters that are invalid in file paths.
    const originalFilename = path.basename(originalFilenameUnsafe).replace(/[^a-zA-Z0-9-._]/g, '_');
    // Sanitize the fileId as it's used to create a directory
    const safeIdentifier = fileId.replace(/[^a-zA-Z0-9-._]/g, '_');

    const chunkBuffer = await request.arrayBuffer();
    if (!chunkBuffer || chunkBuffer.byteLength === 0) {
      console.error(`[SERVER] Received empty chunk for ${safeIdentifier} index ${chunkIndex}`);
      return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
    }

    const chunkDir = path.join(TMP_DIR, safeIdentifier);
    await fs.mkdir(chunkDir, { recursive: true });
    const chunkFilePath = path.join(chunkDir, `${chunkIndex}.chunk`);
    await fs.writeFile(chunkFilePath, Buffer.from(chunkBuffer));

    // Check if all chunks have been received
    const isUploadComplete = (chunkIndex + 1) === totalChunks;

    if (isUploadComplete) {
      const finalFilePath = path.join(UPLOAD_DIR, originalFilename);
      
      try {
        const fileHandle = await fs.open(finalFilePath, 'w');
        for (let i = 0; i < totalChunks; i++) {
          const tempChunkPath = path.join(chunkDir, `${i}.chunk`);
          try {
             const chunkData = await fs.readFile(tempChunkPath);
             await fileHandle.write(chunkData);
          } catch (readError: any) {
            // This is a critical race condition where a chunk might not be fully written yet.
            // Wait a moment and retry once.
            console.warn(`[SERVER] Chunk ${i} not ready for ${safeIdentifier}, retrying... Details: ${readError.message}`);
            await new Promise(resolve => setTimeout(resolve, 500));
            try {
                const chunkData = await fs.readFile(tempChunkPath);
                await fileHandle.write(chunkData);
            } catch(retryError: any) {
                console.error(`[SERVER] FATAL: Retry failed for chunk ${i} of ${safeIdentifier}. Aborting assembly. Details: ${retryError.message}`);
                await fileHandle.close().catch(() => {}); // Attempt to close the handle
                await fs.unlink(finalFilePath).catch(() => {}); // Attempt to delete the partial file
                throw new Error(`Failed to read chunk ${i} on retry.`);
            }
          }
        }
        await fileHandle.close();
      } catch (e: any) {
        console.error(`[SERVER] FATAL: Could not assemble file for ${safeIdentifier}`, e);
        // Cleanup failed assembly
        await fs.rm(chunkDir, { recursive: true, force: true }).catch(err => console.error(`Error cleaning up chunk dir: ${err.message}`));
        return NextResponse.json({ error: `Failed to assemble file: ${e.message}` }, { status: 500 });
      }
      
      // Cleanup successful assembly
      await fs.rm(chunkDir, { recursive: true, force: true }).catch(err => console.error(`Error cleaning up chunk dir: ${err.message}`));
      
      console.log(`[SERVER] Successfully assembled file: ${originalFilename}`);
      return NextResponse.json({ message: "File upload complete.", filename: originalFilename }, { status: 200 });
    }

    return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received.` }, { status: 200 });

  } catch (error: any) {
    console.error("[SERVER] Unhandled error in upload handler:", error.stack);
    const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
