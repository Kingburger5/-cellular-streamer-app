
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

function parseHeader(header: string): Record<string, string> {
    const result: Record<string, string> = {};
    header.split(';').forEach(pair => {
        const separatorIndex = pair.indexOf(':');
        if (separatorIndex > 0) {
            const key = pair.substring(0, separatorIndex).trim();
            const value = pair.substring(separatorIndex + 1).trim();
            result[key] = value;
        }
    });
    return result;
}


export async function POST(request: NextRequest) {
  try {
    await ensureUploadDirExists();

    const userData = request.headers.get("x-userdata");

    if (!userData) {
      console.error("[SERVER] Missing 'x-userdata' header.");
      return NextResponse.json({ error: "Missing required x-userdata header." }, { status: 400 });
    }

    const allHeaders = Object.fromEntries(request.headers.entries());
    console.log(`[SERVER] Received request with headers:`, allHeaders);
    console.log(`[SERVER] Received x-userdata: ${userData}`);

    const parsedHeaders = parseHeader(userData);

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
    // Sanitize the filename to prevent directory traversal attacks
    const originalFilename = path.basename(originalFilenameUnsafe);

    const chunkBuffer = await request.arrayBuffer();
    if (!chunkBuffer || chunkBuffer.byteLength === 0) {
        console.error("[SERVER] Empty chunk received.");
        return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
    }
    const buffer = Buffer.from(chunkBuffer);
    
    if (!chunkStore.has(fileIdentifier)) {
        if (chunkIndex === 0) {
            chunkStore.set(fileIdentifier, []);
        } else {
            console.error(`[SERVER] Received out-of-order chunk for ${originalFilename}. Rejecting.`);
            return NextResponse.json({ error: "Out-of-order chunk received." }, { status: 400 });
        }
    }

    const chunks = chunkStore.get(fileIdentifier)!;
    chunks.push(buffer);

    console.log(`[SERVER] Stored chunk ${chunks.length}/${totalChunks} for ${originalFilename} (ID: ${fileIdentifier})`);

    // If this is the last chunk, assemble the file.
    if (chunks.length === totalChunks) {
        console.log(`[SERVER] All chunks received for ${originalFilename}. Assembling and saving...`);
        
        const fullFileBuffer = Buffer.concat(chunks);
        const finalFilePath = path.join(UPLOAD_DIR, originalFilename);

        // Write the file asynchronously.
        fs.writeFile(finalFilePath, fullFileBuffer).then(() => {
             console.log(`[SERVER] Successfully saved ${originalFilename} to ${finalFilePath}.`);
             // Clean up the in-memory store for this file
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
