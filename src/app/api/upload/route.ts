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

export async function POST(request: NextRequest) {
  try {
    await ensureUploadDirExists();
    const formData = await request.formData();
    const chunk = formData.get("chunk") as File | null;
    const fileIdentifier = formData.get("fileIdentifier") as string | null;
    const chunkIndexStr = formData.get("chunkIndex") as string | null;
    const totalChunksStr = formData.get("totalChunks") as string | null;
    const originalFilename = formData.get("originalFilename") as string | null;

    if (!chunk || !fileIdentifier || !chunkIndexStr || !totalChunksStr || !originalFilename) {
      return NextResponse.json({ error: "Missing required form data." }, { status: 400 });
    }

    const chunkIndex = parseInt(chunkIndexStr);
    const totalChunks = parseInt(totalChunksStr);

    // Sanitize filename to prevent directory traversal
    const safeFilename = path.basename(originalFilename);
    const tempFilePath = path.join(UPLOAD_DIR, `${fileIdentifier}.part`);

    const buffer = Buffer.from(await chunk.arrayBuffer());
    await fs.appendFile(tempFilePath, buffer);

    if (chunkIndex === totalChunks - 1) {
      // Last chunk, rename the file
      const finalFilePath = path.join(UPLOAD_DIR, safeFilename);
      await fs.rename(tempFilePath, finalFilePath);
      console.log(`File uploaded successfully: ${safeFilename}`);
      return NextResponse.json({ message: "File uploaded successfully.", filename: safeFilename }, { status: 200 });
    }

    return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received.` }, { status: 200 });

  } catch (error) {
    console.error("Upload error:", error);
    const errorMessage = error instanceof Error ? error.message : "Internal Server Error";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
