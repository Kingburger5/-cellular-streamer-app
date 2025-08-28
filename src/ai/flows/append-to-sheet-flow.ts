
'use server';
/**
 * @fileOverview A flow for appending data to a Google Sheet. This functionality is currently disabled.
 */

import { ai } from '@/ai/genkit';
import { z } from 'genkit';
import type { DataPoint } from '@/lib/types';

// Define the schema for the flow's input
const AppendToSheetInputSchema = z.object({
  dataPoint: z.custom<DataPoint>(),
  originalFilename: z.string(),
});
export type AppendToSheetInput = z.infer<typeof AppendToSheetInputSchema>;

// This async wrapper is the function that will be called from other server-side code.
export async function appendToSheet(input: AppendToSheetInput): Promise<string> {
    //return appendToSheetFlow(input);
    console.log("Google Sheet functionality is currently disabled.");
    return "Google Sheet functionality is currently disabled.";
}


// The Genkit flow is defined here but NOT exported directly.
const appendToSheetFlow = ai.defineFlow(
  {
    name: 'appendToSheetFlow',
    inputSchema: AppendToSheetInputSchema,
    outputSchema: z.string(), // Returns a success message or error
  },
  async ({ dataPoint, originalFilename }) => {
    return "Google Sheet functionality is currently disabled.";
  }
);
