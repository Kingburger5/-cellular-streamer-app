
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");
const TMP_DIR = path.join(process.cwd(), "tmp");

// Helper to ensure directories exist
async function ensureDirsExist() {
  try {
    await fs.mkdir(UPLOAD_DUR, { recursive: true });
    await fs.mkdir(TMP_DIR, { recursive: true });
  } catch (error: any) {
    if (error.code !== 'EEXIST') {
      console.error("[SERVER] Fatal: Could not create server directories.", error);
      throw new Error("Could not create server directories.");
    }
  }
}

function parseUserDataHeader(header: string | null): Record<string, string> {
    const data: Record<string, string> = {};
    if (!header) {
        return data;
    }
    const pairs = header.split(';').map(s => s.trim());
    for (const pair of pairs) {
        const parts = pair.split(/:(.*)/s);
        if (parts.length === 2) {
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
    
    // The SIM7600G AT+HTTPPARA="USERDATA" sends metadata as a single header named "x-userdata"
    const userData = parseUserDataHeader(request.headers.get("x-userdata"));

    const fileId = userData['x-file-id'];
    const chunkIndexStr = userData['x-chunk-index'];
    const totalChunksStr = userData['x-total-chunks'];
    const originalFilenameUnsafe = userData['x-original-filename'];


    if (!fileId || !chunkIndexStr || !totalChunksStr || !originalFilenameUnsafe) {
        console.error("[SERVER] Missing required headers", { userData });
        return NextResponse.json({ error: "Missing required headers." }, { status: 400 });
    }
    
    const chunkIndex = parseInt(chunkIndexStr);
    const totalChunks = parseInt(totalChunksStr);

    // Sanitize the filename to remove characters that are invalid in file paths.
    const originalFilename = path.basename(originalFilenameUnsafe).replace(/[^a-zA-Z0-9-._]/g, '_');
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
        await fs.unlink(finalFilePath).catch(() => {});
        await fs.rm(chunkDir, { recursive: true, force: true }).catch(() => {});
        return NextResponse.json({ error: `Failed to assemble file.` }, { status: 500 });
      }
      
      await fs.rm(chunkDir, { recursive: true, force: true });
      
      console.log(`[SERVER] Successfully assembled file: ${originalFilename}`);
      return NextResponse.json({ message: "File upload complete.", filename: originalFilename }, { status: 200 });
    }

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
