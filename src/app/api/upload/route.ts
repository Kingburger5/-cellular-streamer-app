
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");
const chunkStore = new Map<string, Buffer[]>();

async function ensureUploadDirExists() {
  try {
    await fs.mkdir(UPLOAD_DIR, { recursive: true });
  } catch (error) {
    console.error("Error creating upload directory:", error);
    throw new Error("Could not create upload directory.");
  }
}

function parseUserDataHeader(header: string): Record<string, string> {
    const result: Record<string, string> = {};
    if (!header) return result;

    const pairs = header.split(';');
    pairs.forEach(pair => {
        const separatorIndex = pair.indexOf(':');
        if (separatorIndex > 0) {
            const key = pair.substring(0, separatorIndex).trim().toLowerCase();
            const value = pair.substring(separatorIndex + 1).trim();
            result[key] = value;
        }
    });
    return result;
}

export async function POST(request: NextRequest) {
  try {
    await ensureUploadDirExists();

    let userData = null;
    request.headers.forEach((value, key) => {
        if (key.toLowerCase().includes('userdata') || value.includes('X-File-ID')) {
            userData = value;
        }
    });
    
    if (!userData) {
        // Fallback for some modules that might use a standard header name
        userData = request.headers.get("x-userdata");
        if(!userData) {
            console.error("[SERVER] Missing USERDATA or equivalent custom header containing 'X-File-ID'.");
            return NextResponse.json({ error: "Missing required USERDATA or x-userdata header." }, { status: 400 });
        }
    }
    
    const headers = parseUserDataHeader(userData);
    
    const fileIdentifier = headers["x-file-id"];
    const chunkIndexStr = headers["x-chunk-index"];
    const totalChunksStr = headers["x-total-chunks"];
    const originalFilenameUnsafe = headers["x-original-filename"];

    if (!fileIdentifier || !chunkIndexStr || !totalChunksStr || !originalFilenameUnsafe) {
      console.error("[SERVER] Missing required fields in parsed USERDATA header.", { headers });
      return NextResponse.json({ error: "Missing required fields in parsed USERDATA header." }, { status: 400 });
    }

    const chunkIndex = parseInt(chunkIndexStr);
    const totalChunks = parseInt(totalChunksStr);
    // Sanitize filename to prevent directory traversal
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
    chunks[chunkIndex] = buffer;

    console.log(`[SERVER] Stored chunk ${chunkIndex + 1}/${totalChunks} for ${fileIdentifier}`);

    if (chunks.length === totalChunks && chunks.every(c => c !== undefined)) {
        console.log(`[SERVER] All chunks received for ${originalFilename}. Assembling and saving...`);
        
        const fullFileBuffer = Buffer.concat(chunks);
        const filePath = path.join(UPLOAD_DIR, originalFilename);

        await fs.writeFile(filePath, fullFileBuffer);

        console.log(`[SERVER] Successfully saved ${originalFilename} to ${filePath}.`);
        
        // Clean up the in-memory store for this file
        chunkStore.delete(fileIdentifier);

        return NextResponse.json({ message: "File uploaded successfully.", filename: originalFilename }, { status: 200 });
    }

    return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received and stored.` }, { status: 200 });

  } catch (error) {
    console.error("[SERVER] Unhandled error in upload handler:", error);
    const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
