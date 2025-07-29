
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");

// A simple in-memory store for chunks.
// In a production environment, you would use a more robust solution like Redis.
const chunkStore = new Map<string, Buffer[]>();

// Ensures the upload directory exists.
async function ensureUploadDirExists() {
  try {
    await fs.mkdir(UPLOAD_DIR, { recursive: true });
  } catch (error) {
    // If the directory already exists, we can ignore the error.
    if (error instanceof Error && 'code' in error && error.code === 'EEXIST') {
      return;
    }
    // For any other error, log it and re-throw.
    console.error("Error creating upload directory:", error);
    throw new Error("Could not create upload directory.");
  }
}

// Parses the semicolon-separated string from the x-userdata header.
function parseUserDataHeader(header: string | null): Record<string, string> {
    const result: Record<string, string> = {};
    if (!header) {
        return result;
    }

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
  // Wrap the entire handler in a try...catch block to prevent server crashes.
  try {
    // Ensure the upload directory exists before doing anything else.
    await ensureUploadDirExists();

    // The SIM7600G sends custom metadata in the 'x-userdata' header.
    const userData = request.headers.get("x-userdata");

    // Log all headers for debugging purposes.
    const allHeaders = JSON.stringify(Object.fromEntries(request.headers.entries()));
    console.log(`[SERVER] Received request with headers: ${allHeaders}`);


    if (!userData) {
      console.error("[SERVER] Missing 'x-userdata' header.");
      return NextResponse.json({ error: "Missing required x-userdata header." }, { status: 400 });
    }
    
    console.log(`[SERVER] Received x-userdata: ${userData}`);
    const headers = parseUserDataHeader(userData);
    
    const fileIdentifier = headers["X-File-ID"];
    const chunkIndexStr = headers["X-Chunk-Index"];
    const totalChunksStr = headers["X-Total-Chunks"];
    const originalFilenameUnsafe = headers["X-Original-Filename"];

    if (!fileIdentifier || !chunkIndexStr || !totalChunksStr || !originalFilenameUnsafe) {
      console.error("[SERVER] Missing required fields in parsed x-userdata header.", { headers });
      return NextResponse.json({ error: "Missing required fields in parsed x-userdata header." }, { status: 400 });
    }

    const chunkIndex = parseInt(chunkIndexStr);
    const totalChunks = parseInt(totalChunksStr);
    // Sanitize the filename to prevent directory traversal attacks.
    const originalFilename = path.basename(originalFilenameUnsafe);

    const chunkBuffer = await request.arrayBuffer();
    if (!chunkBuffer || chunkBuffer.byteLength === 0) {
      console.error("[SERVER] Empty chunk received.");
      return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
    }
    const buffer = Buffer.from(chunkBuffer);
    
    // Initialize the chunk array for this file if it's the first chunk.
    if (!chunkStore.has(fileIdentifier)) {
        // If this is the first chunk, delete any orphaned part files from previous failed uploads.
        const partFilePath = path.join(UPLOAD_DIR, `${originalFilename}.part`);
         try {
            await fs.unlink(partFilePath);
            console.log(`[SERVER] Deleted orphaned part file: ${partFilePath}`);
        } catch (error) {
            if (error instanceof Error && 'code' in error && error.code !== 'ENOENT') {
                console.error(`[SERVER] Error deleting orphaned part file:`, error);
            }
        }
        chunkStore.set(fileIdentifier, []);
    }

    const chunks = chunkStore.get(fileIdentifier)!;
    chunks[chunkIndex] = buffer;

    console.log(`[SERVER] Stored chunk ${chunkIndex + 1}/${totalChunks} for ${originalFilename} (ID: ${fileIdentifier})`);

    // If all chunks have been received...
    if (chunks.length === totalChunks && chunks.every(c => c !== undefined)) {
        console.log(`[SERVER] All chunks received for ${originalFilename}. Assembling and saving...`);
        
        const fullFileBuffer = Buffer.concat(chunks);
        const finalFilePath = path.join(UPLOAD_DIR, originalFilename);

        await fs.writeFile(finalFilePath, fullFileBuffer);

        console.log(`[SERVER] Successfully saved ${originalFilename} to ${finalFilePath}.`);
        
        // Clean up the in-memory chunk store for this file.
        chunkStore.delete(fileIdentifier);

        return NextResponse.json({ message: "File uploaded successfully.", filename: originalFilename }, { status: 200 });
    }

    // If not all chunks are received yet, just acknowledge the reception of the chunk.
    return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received and stored.` }, { status: 200 });

  } catch (error) {
    // If any error occurs, log it and return a generic 500 error.
    console.error("[SERVER] Unhandled error in upload handler:", error);
    const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
