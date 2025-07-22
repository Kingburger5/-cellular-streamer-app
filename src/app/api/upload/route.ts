import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = "/tmp/uploads";
const CHUNK_DIR = "/tmp/uploads/chunks";

async function ensureDirExists(dir: string) {
  try {
    await fs.access(dir);
  } catch {
    await fs.mkdir(dir, { recursive: true });
  }
}

async function reassembleFile(fileIdentifier: string, originalFilename: string) {
    const chunkDir = path.join(CHUNK_DIR, fileIdentifier);
    const finalFilePath = path.join(UPLOAD_DIR, `${Date.now()}-${originalFilename}`);

    try {
        const chunkFilenames = (await fs.readdir(chunkDir)).sort((a, b) => parseInt(a) - parseInt(b));
        const finalFileWriteStream = await fs.open(finalFilePath, 'w');

        for (const chunkFilename of chunkFilenames) {
            const chunkPath = path.join(chunkDir, chunkFilename);
            const chunkBuffer = await fs.readFile(chunkPath);
            await fs.writeFile(finalFileWriteStream, chunkBuffer);
            await fs.unlink(chunkPath); // Clean up chunk
        }

        await finalFileWriteStream.close();
        await fs.rmdir(chunkDir); // Clean up chunk directory

        return finalFilePath;
    } catch (error) {
        console.error("Error reassembling file:", error);
        // Cleanup on error
        try {
            await fs.rm(chunkDir, { recursive: true, force: true });
        } catch (cleanupError) {
            console.error("Error during cleanup:", cleanupError);
        }
        throw new Error("Failed to reassemble file.");
    }
}

export async function POST(request: NextRequest) {
  try {
    const formData = await request.formData();
    const chunk = formData.get("chunk") as File | null;
    const fileIdentifier = formData.get("fileIdentifier") as string | null;
    const chunkIndex = formData.get("chunkIndex") as string | null;
    const totalChunks = formData.get("totalChunks") as string | null;
    const originalFilename = formData.get("originalFilename") as string | null;

    if (!chunk || !fileIdentifier || chunkIndex === null || totalChunks === null || !originalFilename) {
      return NextResponse.json({ error: "Missing required upload parameters." }, { status: 400 });
    }
    
    await ensureDirExists(UPLOAD_DIR);
    const chunkDir = path.join(CHUNK_DIR, fileIdentifier);
    await ensureDirExists(chunkDir);

    const chunkPath = path.join(chunkDir, chunkIndex);
    const chunkBuffer = Buffer.from(await chunk.arrayBuffer());
    await fs.writeFile(chunkPath, chunkBuffer);

    const isLastChunk = parseInt(chunkIndex, 10) === parseInt(totalChunks, 10) - 1;
    if (isLastChunk) {
        const finalPath = await reassembleFile(fileIdentifier, originalFilename);
        return NextResponse.json({ message: "File reassembled successfully.", filename: path.basename(finalPath) }, { status: 201 });
    }

    return NextResponse.json({ message: "Chunk uploaded successfully." }, { status: 200 });
  } catch (error) {
    console.error("Upload error:", error);
    return NextResponse.json(
      { error: "Internal Server Error" },
      { status: 500 }
    );
  }
}
