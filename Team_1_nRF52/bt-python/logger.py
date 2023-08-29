#!.venv/bin/python3
import logging


class MultiLineFormatter(logging.Formatter):
    """Multi-line formatter."""

    def get_header_length(self, record):
        """Get the header length of a given record."""
        return len(super().format(logging.LogRecord(
            name=record.name,
            level=record.levelno,
            pathname=record.pathname,
            lineno=record.lineno,
            msg='', args=(), exc_info=None
        )))

    def format(self, record):
        """Format a record with added indentation."""
        indent = ' ' * self.get_header_length(record)
        head, *trailing = super().format(record).splitlines(True)
        return head + ''.join(indent + line for line in trailing)


logging.basicConfig()
logging.root.handlers[0].formatter = MultiLineFormatter(
    fmt=logging.BASIC_FORMAT
)


def Logger(name: str):
    logger = logging.getLogger(name)
    logger.setLevel(logging.INFO)
    return logger
