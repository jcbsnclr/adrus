use std::collections::{HashMap, HashSet};
use std::fs;
use std::io::{self, prelude::*};
use std::ops::ControlFlow;
use std::path::PathBuf;
use std::process;

use clap::{Parser, Subcommand};

use inquire::Text;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug, Clone, Default)]
struct Ledger {
    notes: Vec<Note>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
enum Condition {
    Has(String),
    Hasnt(String),
}

impl Condition {
    fn check(&self, note: &Note) -> bool {
        match self {
            Condition::Has(key) => note.attrs.contains(key),
            Condition::Hasnt(key) => !note.attrs.contains(key),
        }
    }
}

#[derive(Serialize, Deserialize, Debug, Clone)]
struct Note {
    title: String,
    date: i64,
    attrs: HashSet<String>,
    path: PathBuf,
}

impl Note {
    fn new(title: impl Into<String>, path: impl Into<PathBuf>) -> Note {
        Note {
            title: title.into(),
            date: chrono::Utc::now().timestamp_nanos_opt().unwrap_or(0),
            attrs: HashSet::new(),
            path: path.into(),
        }
    }

    fn with(&mut self, tag: impl Into<String>) -> &mut Note {
        self.attrs.insert(tag.into());
        self
    }

    fn satisfies(&self, conds: &[Condition]) -> bool {
        conds
            .iter()
            .map(|c| c.check(self))
            .reduce(|acc, c| acc && c)
            .unwrap_or(true)
    }
}

#[derive(Parser, Debug)]
struct Cmdline {
    #[command(subcommand)]
    cmd: Command,
}

#[derive(Subcommand, Debug)]
enum Command {
    New,
    Reset,
    List,
    Query {
        #[arg(short = 'H', long, use_value_delimiter = true, value_delimiter = ',')]
        has: Vec<String>,
        #[arg(short = 'n', long)]
        hasnt: Vec<String>,
    },
}

struct State {
    ledger: Ledger,
    filter: Vec<Condition>,
}

fn run(state: &mut State, cmd: Command) -> anyhow::Result<ControlFlow<()>> {
    match cmd {
        Command::List => {
            for note in state
                .ledger
                .notes
                .iter()
                .filter(|n| n.satisfies(&state.filter))
            {
                println!("{note:?}");
            }
        }

        Command::New => {
            let text = inquire::Editor::new("").prompt()?;
            println!("{text}");

            state.ledger.notes.push(Note {
                title: "".into(),
                date: chrono::Utc::now().timestamp_nanos_opt().unwrap_or(0),
                attrs: HashSet::new(),
                path: "a".into(),
            });
        }

        Command::Query { has, hasnt } => {
            for has in has {
                state.filter.push(Condition::Has(has));
            }

            for hasnt in hasnt {
                state.filter.push(Condition::Hasnt(hasnt));
            }
        }

        Command::Reset => state.filter.clear(),

        c => println!("{c:?}"),
    }

    Ok(ControlFlow::Continue(()))
}

fn main() -> anyhow::Result<()> {
    let note1 = Note::new("test post", "abc");
    let mut note2 = Note::new("example post", "def");
    let mut note3 = Note::new("language post", "ghi");

    note2.with("important");
    note3.with("important").with("language");

    let mut state = State {
        ledger: Ledger {
            notes: vec![note1, note2, note3],
        },
        filter: vec![],
    };

    loop {
        let input = Text::new("").prompt()?;
        let args = std::iter::once("adrus").chain(input.split_whitespace());

        match Cmdline::try_parse_from(args) {
            Ok(args) => match run(&mut state, args.cmd)? {
                ControlFlow::Break(()) => break,
                _ => continue,
            },

            Err(err) => println!("{err}"),
        }
    }

    Ok(())
}
