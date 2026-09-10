"""
train_ner.py - Fine-tune DistilBERT for PII Scrubbing on Indian Contexts
"""
import torch
from transformers import (
    AutoTokenizer, 
    AutoModelForTokenClassification, 
    TrainingArguments, 
    Trainer
)
from datasets import Dataset

# Define label mapping for PII
LABEL_LIST = [
    "O", "B-PERSON", "I-PERSON", "B-ADDRESS", "I-ADDRESS", 
    "B-GOV_ID", "I-GOV_ID", "B-CARD", "I-CARD", "B-PHONE", "I-PHONE", "B-ORG", "I-ORG"
]
LABEL_TO_ID = {label: i for i, label in enumerate(LABEL_LIST)}
ID_TO_LABEL = {i: label for i, label in enumerate(LABEL_LIST)}

def get_dummy_data():
    """Generate synthetic Indian PII data for testing the pipeline."""
    return {
        "tokens": [
            ["mera", "naam", "Rahul", "Sharma", "hai", "aur", "mera", "Aadhaar", "hai", "1234", "5678", "9012"],
            ["contact", "me", "at", "+91", "9876543210", "for", "the", "Infosys", "contract"]
        ],
        "ner_tags": [
            [0, 0, 1, 2, 0, 0, 0, 0, 0, 5, 6, 6],
            [0, 0, 0, 9, 10, 0, 0, 11, 0]
        ]
    }

def tokenize_and_align_labels(examples, tokenizer):
    tokenized_inputs = tokenizer(
        examples["tokens"], truncation=True, is_split_into_words=True, padding="max_length", max_length=128
    )
    labels = []
    for i, label in enumerate(examples["ner_tags"]):
        word_ids = tokenized_inputs.word_ids(batch_index=i)
        previous_word_idx = None
        label_ids = []
        for word_idx in word_ids:
            if word_idx is None:
                label_ids.append(-100)
            elif word_idx != previous_word_idx:
                label_ids.append(label[word_idx])
            else:
                label_ids.append(-100)
            previous_word_idx = word_idx
        labels.append(label_ids)
    tokenized_inputs["labels"] = labels
    return tokenized_inputs

def main():
    print("Initializing DistilBERT NER fine-tuning...")
    
    # Check GPU
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Using device: {device}")
    
    model_name = "distilbert-base-uncased"
    tokenizer = AutoTokenizer.from_pretrained(model_name)
    model = AutoModelForTokenClassification.from_pretrained(
        model_name, num_labels=len(LABEL_LIST), id2label=ID_TO_LABEL, label2id=LABEL_TO_ID
    ).to(device)

    # Prepare data
    dataset = Dataset.from_dict(get_dummy_data())
    tokenized_dataset = dataset.map(lambda x: tokenize_and_align_labels(x, tokenizer), batched=True)

    # Training arguments optimized for low VRAM
    training_args = TrainingArguments(
        output_dir="./ner_results",
        learning_rate=2e-5,
        per_device_train_batch_size=2, # Tiny batch size for 4GB VRAM
        num_train_epochs=3,
        weight_decay=0.01,
        evaluation_strategy="no",
        save_strategy="no",
        fp16=torch.cuda.is_available() # Mixed precision to save memory
    )

    trainer = Trainer(
        model=model,
        args=training_args,
        train_dataset=tokenized_dataset,
    )

    print("Starting training...")
    trainer.train()
    
    # Save quantized model
    print("Training complete. Exporting model...")
    trainer.save_model("./ner_final_model")
    tokenizer.save_pretrained("./ner_final_model")
    print("Exported to ./ner_final_model")

if __name__ == "__main__":
    main()
