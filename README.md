# minicoder

**Website**: [https://andrewchambers.github.io/minicoder/](https://andrewchambers.github.io/minicoder/)

**Downloads**: [itch.io/minicoder](https://andrewchambers.itch.io/minicoder)

**Discord**: [Join our community](https://discord.gg/cZk3yxBxRS)

**⚠️ Alpha version** - Be an early tester and supporter!

`minicoder` is a command line tool that uses ai to assist the user at
any task you can do from the command line on unix systems.

![minicoder demo](https://github.com/andrewchambers/minicoder/blob/artifacts/minicoder-demo.gif?raw=true)

It is designed around a few core ideas: 

- Follow good cli and unix design practices.
- Support as many AI models as possible with a universal and simple interface.
- AI advances will keep improving the tool without complicating the tool itself.

## Examples

```
$ minicoder --focus '*.c' 'Can you fix the compile errors in this project?'
```

```
$ minicoder --model openai/o3 'Can you explain the files in the current directory?'
```

## Support the Project

If you're paying for AI model access through OpenAI, Anthropic, or other providers, consider supporting the development of minicoder as well! Your contributions help maintain and improve this tool.

**GitHub Sponsors**: [Sponsor @andrewchambers](https://github.com/sponsors/andrewchambers)

Every contribution, no matter the size, helps keep this project actively maintained and improved.

## Community

Join our Discord community to get help, share ideas, and discuss minicoder development:

**Discord**: [Join the minicoder Discord](https://discord.gg/cZk3yxBxRS)

## User manuals

- [minicoder(1)](doc/minicoder.1.scdoc) - Main manual page
- [minicoder-model-config(5)](doc/minicoder-model-config.5.scdoc) - Model configuration documentation

### Use your own AI models

See [minicoder-model-config(5)](doc/minicoder-model-config.5.scdoc) for details on configuring custom models.

### Sandboxing

Sandboxing is a work in progress.
We aim to provide a simplified interface to limit
the assistant to specific directories and alsto to disable internet access. 

## Building 

## Design notes

### Coding conventions

- The software is written in C to keep it resource light and portable.
- We use a tiny conservative garbage collector to prevent many of C's pitfalls.
- This project is developed with AI assistance and undergoes periodic human review and cleanup.

### Agent loop

- We try to make the agent loop is as simple as possible.
- The model is asked to keep track of its own plans and todos across iterations.
- The model is asked to 'focus' on files as it sees fit (add/remove from the context).
- The model performs actions in the world by generating shell scripts which are run, the output of which is returned to the model.
- There is no model/provider specific tool call convention.
- There is no infitely growing context, instead the model is fed its own compacted summary each iteration.
