
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");

async function ensureUploadDirExists() {
  try {
    await fs.access(UPLOAD_DIR);
  } catch {
    await fs.mkdir(UPLOAD_DIR, { recursive: true });
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
    console.log("[SERVER] Received upload request.");

    let userData = null;
    let foundHeaderKey = null;

    console.log("[SERVER] All incoming headers:");
    request.headers.forEach((value, key) => {
        console.log(`- ${key}: ${value}`);
        // The SIM7600 might be sending USERDATA as the key or part of it
        if (key.toLowerCase().includes('userdata') || value.includes('X-File-ID')) {
            userData = value;
            foundHeaderKey = key;
        }
    });
    
    if (userData) {
         console.log(`[SERVER] Found custom header string in key '${foundHeaderKey}': ${userData}`);
    } else {
        // Fallback for some modules that might use a standard header name
        userData = request.headers.get("x-userdata");
        if(userData) console.log("[SERVER] Found custom header in 'x-userdata'");
    }

    if (!userData) {
        return NextResponse.json({ error: "Missing USERDATA or equivalent custom header." }, { status: 400 });
    }
    
    const headers = parseUserDataHeader(userData);
    
    const fileIdentifier = headers["x-file-id"];
    const chunkIndexStr = headers["x-chunk-index"];
    const totalChunksStr = headers["x-total-chunks"];
    let originalFilename = headers["x-original-filename"];


    console.log(`[SERVER] Parsed Headers: x-file-id=${fileIdentifier}, x-chunk-index=${chunkIndexStr}, x-total-chunks=${totalChunksStr}, x-original-filename=${originalFilename}`);

    if (!fileIdentifier || !chunkIndexStr || !totalChunksStr || !originalFilename) {
      console.error("[SERVER] Missing required fields in parsed USERDATA header.");
      return NextResponse.json({ error: "Missing required fields in parsed USERDATA header." }, { status: 400 });
    }

    const chunkIndex = parseInt(chunkIndexStr);
    const totalChunks = parseInt(totalChunksStr);

    const chunkBuffer = await request.arrayBuffer();
    if (!chunkBuffer || chunkBuffer.byteLength === 0) {
      console.error("[SERVER] Empty chunk received.");
      return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
    }
    const buffer = Buffer.from(chunkBuffer);
    console.log(`[SERVER] Received chunk ${chunkIndex + 1}/${totalChunks} with size ${buffer.length} bytes.`);

    // Sanitize filename to prevent directory traversal
    const safeFilename = path.basename(originalFilename);
    const tempFilePath = path.join(UPLOAD_DIR, `${fileIdentifier}.part`);

    // **CLEANUP LOGIC**: If this is the first chunk, delete any stale .part file.
    if (chunkIndex === 0) {
        try {
            await fs.unlink(tempFilePath);
            console.log(`[SERVER] Deleted stale part file: ${tempFilePath}`);
        } catch (error: any) {
            if (error.code !== 'ENOENT') { // Ignore "file not found" errors
                console.error(`[SERVER] Error deleting stale part file:`, error);
                throw error;
            }
        }
    }
    
    await fs.appendFile(tempFilePath, buffer);
    console.log(`[SERVER] Appended chunk ${chunkIndex + 1} to ${tempFilePath}`);

    if (chunkIndex === totalChunks - 1) {
      // Last chunk, rename the file
      const finalFilePath = path.join(UPLOAD_DIR, safeFilename);
      try {
        // Overwrite final file if it exists
        await fs.rename(tempFilePath, finalFilePath);
        console.log(`[SERVER] File upload completed. Renamed ${tempFilePath} to ${finalFilePath}`);
      } catch (renameError) {
          console.error('[SERVER] Error renaming file:', renameError);
          // Try to clean up the part file on failure
          try {
            await fs.unlink(tempFilePath);
          } catch (cleanupError) {
             console.error('[SERVER] Error cleaning up part file after failed rename:', cleanupError);
          }
          throw renameError;
      }
      return NextResponse.json({ message: "File uploaded successfully.", filename: safeFilename }, { status: 200 });
    }

    return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received.` }, { status: 200 });

  } catch (error) {
    console.error("[SERVER] Upload error:", error);
    const errorMessage = error instanceof Error ? error.message : "Internal Server Error";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
