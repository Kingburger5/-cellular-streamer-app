
"use client";

import { useState, useEffect } from 'react';

interface FormattedDateProps {
    date: Date;
}

export function FormattedDate({ date }: FormattedDateProps) {
  const [formattedDate, setFormattedDate] = useState<string>('');

  useEffect(() => {
    // Check if date is valid before formatting
    if (date && !isNaN(date.getTime())) {
        setFormattedDate(new Date(date).toLocaleString("en-US", {
            year: 'numeric',
            month: 'short',
            day: 'numeric',
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit',
            hour12: true
        }));
    } else {
        setFormattedDate('Invalid Date');
    }
  }, [date]);

  return <span>{formattedDate}</span>;
}
