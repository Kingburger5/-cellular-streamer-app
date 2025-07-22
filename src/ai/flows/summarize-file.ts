 'use server';

/**
 * @fileOverview An AI agent for summarizing files.
 *
 * - summarizeFile - A function that generates a summary of a file.
 * - SummarizeFileInput - The input type for the summarizeFile function.
 * - SummarizeFileOutput - The return type for the summarizeFile function.
 */

import {ai} from '@/ai/genkit';
import {z} from 'genkit';

const SummarizeFileInputSchema = z.object({
  fileContent: z.string().describe('The content of the file to summarize.'),
  filename: z.string().describe('The name of the file.'),
});
export type SummarizeFileInput = z.infer<typeof SummarizeFileInputSchema>;

const SummarizeFileOutputSchema = z.object({
  summary: z.string().describe('A short summary of the file content.'),
});
export type SummarizeFileOutput = z.infer<typeof SummarizeFileOutputSchema>;

export async function summarizeFile(input: SummarizeFileInput): Promise<SummarizeFileOutput> {
  return summarizeFileFlow(input);
}

const prompt = ai.definePrompt({
  name: 'summarizeFilePrompt',
  input: {schema: SummarizeFileInputSchema},
  output: {schema: SummarizeFileOutputSchema},
  prompt: `You are an AI expert in summarizing files. Please provide a concise summary of the following file content:

Filename: {{{filename}}}
Content: {{{fileContent}}}

Summary:`,
});

const summarizeFileFlow = ai.defineFlow(
  {
    name: 'summarizeFileFlow',
    inputSchema: SummarizeFileInputSchema,
    outputSchema: SummarizeFileOutputSchema,
  },
  async input => {
    const {output} = await prompt(input);
    return output!;
  }
);
