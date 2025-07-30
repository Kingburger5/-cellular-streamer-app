
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";
import os from "os";
import { database } from '@/lib/firebase';
import { ref, set, serverTimestamp } from 'firebase/database';


// Helper function to read the entire request stream into a buffer.
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

// Function to update the status in Firebase Realtime Database
async function updateStatus(identifier: string, status: string, error: string | null = null) {
    if (!identifier || identifier === 'unknown') return;
    try {
        const dbRef = ref(database, 'uploads/' + identifier);
        await set(dbRef, {
            status,
            error,
            lastUpdated: serverTimestamp()
        });
    } catch (dbError: any) {
        console.error(`[SERVER] Firebase status update failed for ${identifier}: ${dbError.message}`);
    }
}


export async function POST(request: NextRequest) {
  let fileIdentifier = 'unknown';
  try {
    const TMP_DIR = path.join(os.tmpdir(), "cellular-uploads");
    await fs.mkdir(TMP_DIR, { recursive: true });

    const userDataHeader = request.headers.get("x-userdata");

    if (!userDataHeader) {
      await updateStatus(fileIdentifier, 'Request failed', 'Missing x-userdata header.');
      return NextResponse.json({ error: "Missing x-userdata header." }, { status: 400 });
    }
    
    const data: Record<string, string> = {};
    const pairs = userDataHeader.split(';').map(s => s.trim());
    for (const pair of pairs) {
        const parts = pair.split(/:(.*)/s);
        if (parts.length === 2) {
            const key = parts[0].trim().toLowerCase();
            const value = parts[1].trim();
            data[key] = value;
        }
    }

    const fileId = data['x-file-id'];
    const chunkIndexStr = data['x-chunk-index'];
    const totalChunksStr = data['x-total-chunks'];
    const originalFilenameUnsafe = data['x-original-filename'];

    // Use a sanitized version for logging from the start.
    fileIdentifier = fileId?.replace(/[^a-zA-Z0-9-._]/g, '_') || 'unknown';
    
    await updateStatus(fileIdentifier, `Received chunk ${parseInt(chunkIndexStr, 10) + 1} / ${totalChunksStr}`);

    if (!fileId || !chunkIndexStr || !totalChunksStr || !originalFilenameUnsafe) {
        await updateStatus(fileIdentifier, 'Request failed', 'Missing required fields in x-userdata header.');
        return NextResponse.json({ error: "Missing one or more required fields in x-userdata header." }, { status: 400 });
    }
    
    if (!request.body) {
        await updateStatus(fileIdentifier, 'Request failed', 'Empty request body.');
        return NextResponse.json({ error: "Empty request body." }, { status: 400 });
    }
    const chunkBuffer = await streamToBuffer(request.body);
    
    if (!chunkBuffer || chunkBuffer.byteLength === 0) {
        await updateStatus(fileIdentifier, 'Request failed', 'Empty chunk received.');
        return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
    }
    
    const chunkIndex = parseInt(chunkIndexStr, 10);
    const totalChunks = parseInt(totalChunksStr, 10);
    const originalFilename = path.basename(originalFilenameUnsafe).replace(/[^a-zA-Z0-9-._]/g, '_');

    const chunkDir = path.join(TMP_DIR, fileIdentifier);
    await fs.mkdir(chunkDir, { recursive: true });

    const chunkFilePath = path.join(chunkDir, `${chunkIndex}.chunk`);
    await fs.writeFile(chunkFilePath, chunkBuffer);

    if ((chunkIndex + 1) === totalChunks) {
      await updateStatus(fileIdentifier, 'All chunks received. Assembling file...');
      const finalUploadDir = path.join(process.cwd(), "uploads");
      await fs.mkdir(finalUploadDir, { recursive: true });
      const finalFilePath = path.join(finalUploadDir, originalFilename);
      
      const fileHandle = await fs.open(finalFilePath, 'w');
      for (let i = 0; i < totalChunks; i++) {
        const tempChunkPath = path.join(chunkDir, `${i}.chunk`);
        const chunkData = await fs.readFile(tempChunkPath);
        await fileHandle.write(chunkData);
      }
      await fileHandle.close();
      
      await fs.rm(chunkDir, { recursive: true, force: true });
      
      await updateStatus(fileIdentifier, `Complete: ${originalFilename}`);
      return NextResponse.json({ message: "File upload complete.", filename: originalFilename }, { status: 200 });
    }

    return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received.` }, { status: 200 });

  } catch (error: any) {
    console.error("[SERVER] Unhandled error in upload handler:", error.stack);
    const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
    await updateStatus(fileIdentifier, 'Server error', errorMessage);
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
