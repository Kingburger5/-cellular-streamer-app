
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";
import os from "os";

// A robust function to parse the specific "key: value; key: value" format
// from the SIM7600 USERDATA header. It is case-insensitive and safe.
function parseUserDataHeader(header: string | null): Record<string, string> {
    const data: Record<string, string> = {};
    if (!header) {
        return data;
    }
    // Split by semicolon to get "key: value" pairs
    const pairs = header.split(';').map(s => s.trim());
    for (const pair of pairs) {
        // Split by the first colon to separate key and value
        const parts = pair.split(/:(.*)/s);
        if (parts.length === 2) {
            // Standardize the key to lowercase to handle any casing issues
            const key = parts[0].trim().toLowerCase();
            const value = parts[1].trim();
            data[key] = value;
        }
    }
    return data;
}


// Helper function to read the entire request stream into a buffer.
// This is necessary because request.arrayBuffer() can be unreliable with streamed sources.
async function streamToBuffer(stream: ReadableStream<Uint8Array>): Promise<Buffer> {
    const reader = stream.getReader();
    const chunks: Uint8Array[] = [];
    while (true) {
        const { done, value } = await reader.read();
        if (done) {
            break;
        }
        chunks.push(value);
    }
    return Buffer.concat(chunks);
}


export async function POST(request: NextRequest) {
  try {
    // Use a directory that is guaranteed to be writable in serverless environments
    const TMP_DIR = path.join(os.tmpdir(), "cellular-uploads");
    await fs.mkdir(TMP_DIR, { recursive: true });

    // Next.js lowercases all incoming header names.
    const userData = parseUserDataHeader(request.headers.get("x-userdata"));

    const fileId = userData['x-file-id'];
    const chunkIndexStr = userData['x-chunk-index'];
    const totalChunksStr = userData['x-total-chunks'];
    const originalFilenameUnsafe = userData['x-original-filename'];

    if (!fileId || !chunkIndexStr || !totalChunksStr || !originalFilenameUnsafe) {
        console.error("[SERVER] Missing required headers", { userData });
        return NextResponse.json({ error: "Missing one or more required headers: x-file-id, x-chunk-index, x-total-chunks, x-original-filename." }, { status: 400 });
    }
    
    // Read the body as a stream, which is more robust.
    if (!request.body) {
         return NextResponse.json({ error: "Empty request body." }, { status: 400 });
    }
    const chunkBuffer = await streamToBuffer(request.body);
    
    if (!chunkBuffer || chunkBuffer.byteLength === 0) {
      return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
    }
    
    const chunkIndex = parseInt(chunkIndexStr, 10);
    const totalChunks = parseInt(totalChunksStr, 10);

    // Sanitize identifiers to be safe for use as file/directory names
    const safeIdentifier = fileId.replace(/[^a-zA-Z0-9-._]/g, '_');
    const originalFilename = path.basename(originalFilenameUnsafe).replace(/[^a-zA-Z0-9-._]/g, '_');

    // Create a unique directory for this specific upload
    const chunkDir = path.join(TMP_DIR, safeIdentifier);
    await fs.mkdir(chunkDir, { recursive: true });

    // Write the current chunk to its own file
    const chunkFilePath = path.join(chunkDir, `${chunkIndex}.chunk`);
    await fs.writeFile(chunkFilePath, chunkBuffer);

    // Check if this is the last chunk
    const isUploadComplete = (chunkIndex + 1) === totalChunks;

    if (isUploadComplete) {
      const finalUploadDir = path.join(process.cwd(), "uploads");
      await fs.mkdir(finalUploadDir, { recursive: true });
      const finalFilePath = path.join(finalUploadDir, originalFilename);
      
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
             console.warn(`[SERVER] Chunk ${i} not ready for ${safeIdentifier}, retrying...`);
             await new Promise(resolve => setTimeout(resolve, 500));
             try {
                 const chunkData = await fs.readFile(tempChunkPath);
                 await fileHandle.write(chunkData);
             } catch(retryError: any) {
                 console.error(`[SERVER] FATAL: Retry failed for chunk ${i} of ${safeIdentifier}.`);
                 await fileHandle.close().catch(() => {}); // Attempt to close the handle
                 await fs.unlink(finalFilePath).catch(() => {}); // Attempt to delete the partial file
                 throw new Error(`Failed to read chunk ${i} on retry.`);
             }
          }
        }
        await fileHandle.close();
      } catch (e: any) {
        console.error(`[SERVER] FATAL: Could not assemble file for ${safeIdentifier}`, e);
        // Best-effort cleanup of temporary directory
        await fs.rm(chunkDir, { recursive: true, force: true }).catch(err => console.error(`Error cleaning up chunk dir: ${err.message}`));
        return NextResponse.json({ error: `Failed to assemble file: ${e.message}` }, { status: 500 });
      }
      
      // Best-effort cleanup of temporary directory on success
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
