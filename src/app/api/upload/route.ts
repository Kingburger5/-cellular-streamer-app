
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");

// A simple in-memory store for chunks.
const chunkStore = new Map<string, Buffer[]>();

// Ensures the upload directory exists.
async function ensureUploadDirExists() {
  try {
    await fs.mkdir(UPLOAD_DIR, { recursive: true });
  } catch (error: any) {
    if (error.code !== 'EEXIST') {
      console.error("[SERVER] Error creating upload directory:", error);
      throw new Error("Could not create upload directory.");
    }
  }
}

// Simplified and more robust header parser
function parseUserDataHeader(header: string): Record<string, string> {
    const result: Record<string, string> = {};
    header.split(';').forEach(pair => {
        const parts = pair.split(':');
        if (parts.length === 2) {
            const key = parts[0].trim();
            const value = parts[1].trim();
            result[key] = value;
        }
    });
    return result;
}


export async function POST(request: NextRequest) {
  try {
    await ensureUploadDirExists();

    const allHeaders = Object.fromEntries(request.headers.entries());
    console.log(`[SERVER] Received request with headers:`, allHeaders);
    
    const userData = request.headers.get("x-userdata");

    if (!userData) {
      console.error("[SERVER] Missing 'x-userdata' header.");
      return NextResponse.json({ error: "Missing required x-userdata header." }, { status: 400 });
    }
    
    console.log(`[SERVER] Received x-userdata: ${userData}`);

    const parsedHeaders = parseUserDataHeader(userData);

    const fileIdentifier = parsedHeaders["X-File-ID"];
    const chunkIndexStr = parsedHeaders["X-Chunk-Index"];
    const totalChunksStr = parsedHeaders["X-Total-Chunks"];
    const originalFilenameUnsafe = parsedHeaders["X-Original-Filename"];
    
    if (!fileIdentifier || !chunkIndexStr || !totalChunksStr || !originalFilenameUnsafe) {
        const error = "[SERVER] Missing required fields in parsed x-userdata header.";
        console.error(error, { parsedHeaders });
        return NextResponse.json({ error: error }, { status: 400 });
    }

    const chunkIndex = parseInt(chunkIndexStr);
    const totalChunks = parseInt(totalChunksStr);
    const originalFilename = path.basename(originalFilenameUnsafe);

    const chunkBuffer = await request.arrayBuffer();
    if (!chunkBuffer || chunkBuffer.byteLength === 0) {
        console.error("[SERVER] Empty chunk received.");
        return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
    }
    const buffer = Buffer.from(chunkBuffer);
    
    if (!chunkStore.has(fileIdentifier)) {
        chunkStore.set(fileIdentifier, []);
    }

    const chunks = chunkStore.get(fileIdentifier)!;
    chunks.push(buffer);

    console.log(`[SERVER] Stored chunk ${chunks.length}/${totalChunks} for ${originalFilename} (ID: ${fileIdentifier})`);

    if (chunks.length === totalChunks) {
      console.log(`[SERVER] All chunks received for ${originalFilename}. Assembling and saving...`);
      const fullFileBuffer = Buffer.concat(chunks);
      const finalFilePath = path.join(UPLOAD_DIR, originalFilename);

      // Asynchronously write file and don't await it to send response faster
      fs.writeFile(finalFilePath, fullFileBuffer).then(() => {
          console.log(`[SERVER] Successfully saved ${originalFilename} to ${finalFilePath}.`);
          chunkStore.delete(fileIdentifier);
      }).catch(err => {
          console.error(`[SERVER] Error writing final file ${originalFilename}:`, err);
          chunkStore.delete(fileIdentifier);
      });

      return NextResponse.json({ message: "File upload complete. Processing.", filename: originalFilename }, { status: 200 });
    }

    return NextResponse.json({ message: `Chunk ${chunks.length}/${totalChunks} received.` }, { status: 200 });

  } catch (error) {
    console.error("[SERVER] Unhandled error in upload handler:", error);
    const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
