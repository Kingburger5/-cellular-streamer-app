
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");
const TMP_DIR = path.join(process.cwd(), "tmp");

// Helper to ensure directories exist
async function ensureDirsExist() {
  try {
    await fs.mkdir(UPLOAD_DIR, { recursive: true });
    await fs.mkdir(TMP_DIR, { recursive: true });
  } catch (error: any) {
    if (error.code !== 'EEXIST') {
      console.error("[SERVER] Fatal: Could not create server directories.", error);
      throw new Error("Could not create server directories.");
    }
  }
}

export async function POST(request: NextRequest) {
  try {
    await ensureDirsExist();
    
    const { searchParams } = new URL(request.url);

    const fileId = searchParams.get('fileId');
    const chunkIndexStr = searchParams.get('chunkIndex');
    const totalChunksStr = searchParams.get('totalChunks');
    const originalFilenameUnsafe = searchParams.get('originalFilename');

    if (!fileId || !chunkIndexStr || !totalChunksStr || !originalFilenameUnsafe) {
        console.error("[SERVER] Missing required query parameters", { 
            fileId, chunkIndexStr, totalChunksStr, originalFilenameUnsafe 
        });
        return NextResponse.json({ error: "Missing required query parameters." }, { status: 400 });
    }

    const chunkIndex = parseInt(chunkIndexStr);
    const totalChunks = parseInt(totalChunksStr);
    const originalFilename = path.basename(originalFilenameUnsafe);
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

    // If all chunks have been received, assemble the file
    if (chunkIndex + 1 === totalChunks) {
      const finalFilePath = path.join(UPLOAD_DIR, originalFilename);
      
      try {
        const fileHandle = await fs.open(finalFilePath, 'w');
        for (let i = 0; i < totalChunks; i++) {
          const tempChunkPath = path.join(chunkDir, `${i}.chunk`);
          // It's possible for a chunk to not have been written yet.
          // Add a small delay and retry.
          try {
             const chunkData = await fs.readFile(tempChunkPath);
             await fileHandle.write(chunkData);
          } catch (readError) {
             console.log(`[SERVER] Chunk ${i} not ready for ${safeIdentifier}, retrying...`);
             await new Promise(resolve => setTimeout(resolve, 500));
             const chunkData = await fs.readFile(tempChunkPath);
             await fileHandle.write(chunkData);
          }
        }
        await fileHandle.close();
      } catch (e) {
        console.error(`[SERVER] FATAL: Could not assemble file for ${safeIdentifier}`, e);
        // Clean up potentially corrupted file
        await fs.unlink(finalFilePath).catch(() => {});
        // Also clean up temp chunks
        await fs.rm(chunkDir, { recursive: true, force: true }).catch(() => {});
        return NextResponse.json({ error: `Failed to assemble file.` }, { status: 500 });
      }
      
      await fs.rm(chunkDir, { recursive: true, force: true });
      
      console.log(`[SERVER] Successfully assembled file: ${originalFilename}`);
      return NextResponse.json({ message: "File upload complete.", filename: originalFilename }, { status: 200 });
    }

    // Acknowledge receipt of the chunk
    return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received.` }, { status: 200 });

  } catch (error) {
    console.error("[SERVER] Unhandled error in upload handler:", error);
    const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
