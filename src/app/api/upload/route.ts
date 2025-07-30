
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
      console.error("[SERVER] Error creating directories:", error);
      throw new Error("Could not create server directories.");
    }
  }
}

// Custom parser for the USERDATA header format.
function parseUserDataHeader(header: string | null): Record<string, string> {
    const data: Record<string, string> = {};
    if (!header) return data;

    // The header value is a single string like:
    // "X-File-ID: ...; X-Chunk-Index: ...; ..."
    header.split(';').forEach(part => {
        const firstColonIndex = part.indexOf(':');
        if (firstColonIndex !== -1) {
            const key = part.substring(0, firstColonIndex).trim().toLowerCase();
            const value = part.substring(firstColonIndex + 1).trim();
            data[key] = value;
        }
    });
    return data;
}

export async function POST(request: NextRequest) {
  let fileIdentifier = "unknown_request";
  
  try {
    await ensureDirsExist();
    
    // The SIM7600 AT+HTTPPARA="USERDATA" command sends the data in a proprietary way.
    // Next.js correctly interprets this and makes it available as the 'x-userdata' header.
    const userDataHeader = request.headers.get("x-userdata");

    if (!userDataHeader) {
      console.error("[SERVER] Fatal: Missing x-userdata header.", { headers: request.headers });
      return NextResponse.json({ error: "Fatal: Missing x-userdata header." }, { status: 400 });
    }
   
    const parsedHeaders = parseUserDataHeader(userDataHeader);
    
    const chunkIndexStr = parsedHeaders["x-chunk-index"];
    const totalChunksStr = parsedHeaders["x-total-chunks"];
    const originalFilenameUnsafe = parsedHeaders["x-original-filename"];
    fileIdentifier = parsedHeaders["x-file-id"] || fileIdentifier;

    if (!chunkIndexStr || !totalChunksStr || !originalFilenameUnsafe || !fileIdentifier) {
      console.error("[SERVER] Missing required fields in parsed headers:", parsedHeaders);
      return NextResponse.json({ error: "Missing required fields in x-userdata header." }, { status: 400 });
    }
    
    const chunkIndex = parseInt(chunkIndexStr);
    const totalChunks = parseInt(totalChunksStr);
    
    // Sanitize filename to prevent directory traversal attacks.
    const originalFilename = path.basename(originalFilenameUnsafe);
    const safeIdentifier = fileIdentifier.replace(/[^a-zA-Z0-9-._]/g, '_');

    const chunkBuffer = await request.arrayBuffer();
    if (!chunkBuffer || chunkBuffer.byteLength === 0) {
      return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
    }

    // Write chunk to a temporary file
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
          const chunkData = await fs.readFile(tempChunkPath);
          await fileHandle.write(chunkData);
        }
        await fileHandle.close();
      } catch (e) {
        console.error(`[SERVER] FATAL: Could not assemble file for ${safeIdentifier}`, e);
        // Clean up potentially corrupted file
        await fs.unlink(finalFilePath).catch(() => {});
        return NextResponse.json({ error: `Failed to assemble file.` }, { status: 500 });
      }
      
      // Clean up temporary chunk directory
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
